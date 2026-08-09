#include "RoofMarshalZombie.h"

#include "../AudioSystem.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../ResourceKeys.h"
#include <algorithm>
#include <array>

namespace {
	constexpr int kBodyHealth = 12000;                    // 主人确认的首领本体生命值
	constexpr int kPlantAshDamageCap = 1800;              // 灰烬与土豆雷的单次基础伤害上限
	constexpr int kPlantInstantKillFallbackDamage = 1800; // 大嘴花直杀失败后结算的单次基础伤害
	constexpr float kBossVisualScale = 1.2f;              // 须与 gamedata.json 的督军 scale 同改；影子按此倍率同步放大
	constexpr int kHighThreatHealthThreshold = 8000;      // 低于此本体生命后，高威胁原版池开始参与抽取
	constexpr int kDesperateHealthThreshold = 4000;       // 低于此本体生命后，切换 7 秒四只的最终阶段
	constexpr float kFirstSummonDelay = 1.0f;             // 登场到第一批实际生成的游戏秒数
	constexpr float kNormalSummonInterval = 9.0f;         // 常态与第二阶段的实际召唤间隔，单位：游戏秒
	constexpr float kDesperateSummonInterval = 7.0f;      // 4000 血以下的实际召唤间隔，单位：游戏秒
	constexpr float kCommandPoseDuration = 1.2f;          // 每次召唤后停步播放 anim_idle 的游戏秒数
	constexpr float kCommandBlendTime = 0.2f;             // 指挥姿势与走路轨道切换的混合时长，单位：秒
	constexpr float kLaneSwitchInterval = 6.0f;           // 4000 血以上每次自主换到相邻行的间隔，单位：游戏秒
	constexpr float kLaneTransitionDuration = 0.65f;      // 换行视觉从旧行平滑收敛到新行的时长，单位：游戏秒
	constexpr float kLaneTransitionBlendTime = 0.15f;     // 换行开始/结束时切换走路与待机轨道的混合时长，单位：秒
	constexpr int kNormalSummonCount = 3;                 // 4000 血以上每批生成数量
	constexpr int kDesperateSummonCount = 4;              // 4000 血以下每批生成数量
	constexpr int kHighThreatRollPercent = 30;            // 第二、三阶段每个位置抽取高威胁池的概率
	constexpr float kCommandSpawnX = 910.0f;              // 主人指定的部队召唤 X；直接出现在战场右侧可见区域，单位：像素
	constexpr int kWeatherCommandCadence = 3;             // 每完成多少次实际指挥召唤尝试一次周期改天
	constexpr float kCommandedMediumRainDuration = 20.0f; // 周期天气技能强制中雨的最短游戏秒数
	constexpr float kDesperateHeavyRainDuration = 30.0f;  // 首次进入残血阶段时强制大雨的最短游戏秒数

	// 显式白名单是跨版本契约：第六大关新增或其他原创类型不会因注册到枚举/图鉴而自动混入。
	constexpr std::array<ZombieType, 6> kStandardSummonPool = {
		ZombieType::ZOMBIE_NORMAL,
		ZombieType::ZOMBIE_TRAFFIC_CONE,
		ZombieType::ZOMBIE_POLEVAULTER,
		ZombieType::ZOMBIE_BUCKET,
		ZombieType::ZOMBIE_NEWSPAPER,
		ZombieType::ZOMBIE_DOOR,
	};
	constexpr std::array<ZombieType, 11> kHighThreatSummonPool = {
		ZombieType::ZOMBIE_FOOTBALL,
		ZombieType::ZOMBIE_DANCER,
		ZombieType::ZOMBIE_ZAMBONI,
		ZombieType::ZOMBIE_JACK_IN_THE_BOX,
		ZombieType::ZOMBIE_BALLOON,
		ZombieType::ZOMBIE_DIGGER,
		ZombieType::ZOMBIE_POGO,
		ZombieType::ZOMBIE_BUNGEE,
		ZombieType::ZOMBIE_LADDER,
		ZombieType::ZOMBIE_CATAPULT,
		ZombieType::ZOMBIE_GARGANTUAR,
	};

	template <std::size_t N>
	bool ContainsZombieType(const std::array<ZombieType, N>& pool, ZombieType type)
	{
		return std::find(pool.begin(), pool.end(), type) != pool.end();
	}
}

/**
 * @brief 复用普通僵尸已有的走路、啃食和死亡帧事件，不注册新的动画事件。
 */
void RoofMarshalZombie::SetupZombie()
{
	// 完整复用普通僵尸的帧事件和动作时序，只覆盖首领耐久。
	Zombie::SetupZombie();
	mBodyHealth = kBodyHealth;
	mBodyMaxHealth = kBodyHealth;
	mCommandPhase = CommandPhase::ADVANCING;
	mSummonTimer = kFirstSummonDelay;
	mCommandPoseTimer = 0.0f;
	mLaneSwitchTimer = kLaneSwitchInterval;
	mLaneTransitionRemaining = 0.0f;
	mLaneVisualOffsetY = 0.0f;
	mCommandCount = 0;
	mLaneSwitchCount = 0;
	mLastSummonCount = 0;
	mLastSummonRowMask = 0;
	mLastSummonedTypes.fill(ZombieType::NUM_ZOMBIE_TYPES);
	if (mPoolShadow) {
		// 普通影子默认 (1.0, 0.75)；保持扁率并同步 1.2 倍本体，脚底中心仍由逻辑 Transform 决定。
		mPoolShadow->SetScale(Vector(kBossVisualScale, 0.75f * kBossVisualScale));
	}
	if (!mIsPreview) PlayTrack("anim_idle2", 0.0f, 0.0f);
}

void RoofMarshalZombie::ZombieUpdate(float scaledTime)
{
	if (mIsDying || mIsDead || scaledTime <= 0.0f) return;

	UpdateLaneTransition(scaledTime);
	if (mBodyHealth >= kDesperateHealthThreshold) {
		mLaneSwitchTimer -= scaledTime;
		if (mLaneSwitchTimer <= 0.0f
			&& mCommandPhase == CommandPhase::ADVANCING
			&& mLaneTransitionRemaining <= 0.0f) {
			BeginLaneSwitch();
		}
	}
	else {
		mLaneSwitchTimer = 0.0f;
	}

	// 一旦进入最终阶段，尚未开始的长冷却立刻收敛到 7 秒，而不是再等完旧的 9 秒。
	if (mBodyHealth < kDesperateHealthThreshold
		&& mSummonTimer > kDesperateSummonInterval) {
		mSummonTimer = kDesperateSummonInterval;
	}
	mSummonTimer -= scaledTime;

	if (mCommandPhase == CommandPhase::COMMANDING) {
		mCommandPoseTimer -= scaledTime;
		if (mCommandPoseTimer <= 0.0f) {
			mCommandPoseTimer = 0.0f;
			mCommandPhase = CommandPhase::ADVANCING;
			PlayWalkAnimation(kCommandBlendTime);
		}
	}

	if (mSummonTimer > 0.0f) return;
	SummonCommandedZombies();
	mSummonTimer = mBodyHealth < kDesperateHealthThreshold
		? kDesperateSummonInterval
		: kNormalSummonInterval;
	BeginCommandPose();
}

Vector RoofMarshalZombie::GetVisualPosition() const
{
	return Zombie::GetVisualPosition() + Vector(0.0f, mLaneVisualOffsetY);
}

void RoofMarshalZombie::StartEat(ColliderComponent* other)
{
	if (mCommandPhase == CommandPhase::COMMANDING) return;
	Zombie::StartEat(other);
}

void RoofMarshalZombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	if (mCommandPhase == CommandPhase::COMMANDING
		|| mBodyHealth >= kDesperateHealthThreshold) return;
	Zombie::ZombieMove(scaledDelta, transform);
}

void RoofMarshalZombie::PlayWalkAnimation(float blendTime)
{
	if (mBodyHealth >= kDesperateHealthThreshold
		&& mLaneTransitionRemaining <= 0.0f) {
		PlayTrack("anim_idle2", 0.0f, blendTime);
		return;
	}
	Zombie::PlayWalkAnimation(blendTime);
}

void RoofMarshalZombie::TakeBodyDamage(int damage)
{
	const int healthBeforeHit = mBodyHealth;
	// 临时关闭基类 1/3 掉头，只复用其扣血与断臂逻辑；本函数结束前恢复标志，存档契约不变。
	const bool needDropHead = mNeedDropHead;
	mNeedDropHead = false;
	Zombie::TakeBodyDamage(damage);
	mNeedDropHead = needDropHead;

	if (healthBeforeHit >= kDesperateHealthThreshold
		&& mBodyHealth < kDesperateHealthThreshold) {
		// 最终阶段原子终止尚未完成的自主换行，视觉与逻辑行立即重新对齐并开始亲自推进。
		mLaneSwitchTimer = 0.0f;
		mLaneTransitionRemaining = 0.0f;
		mLaneVisualOffsetY = 0.0f;
		if (mCommandPhase != CommandPhase::COMMANDING && !mIsEating) {
			PlayWalkAnimation(kCommandBlendTime);
		}
		if (mBodyHealth > 0 && mBoard) {
			// 残血阶段只会跨越一次；若已经是大雨，则一次性补足而不会在后续受击中刷新。
			mBoard->TriggerRoofMarshalWeather(
				RainIntensity::HEAVY, kDesperateHeavyRainDuration, true);
		}
	}

	// 督军没有残血掉头阶段；只有致命一击才触发专属掉头，下一帧由基类进入死亡动画。
	if (needDropHead && mHasHead && mBodyHealth == 0) {
		HeadDrop();
		mHasHead = false;
		ClearButter();
	}
}

ZombieType RoofMarshalZombie::RollSummonedZombieType() const
{
	const bool canRollHighThreat = mBodyHealth < kHighThreatHealthThreshold;
	if (canRollHighThreat && GameRandom::Range(1, 100) <= kHighThreatRollPercent) {
		return kHighThreatSummonPool[static_cast<std::size_t>(
			GameRandom::Range(0, static_cast<int>(kHighThreatSummonPool.size()) - 1))];
	}
	return kStandardSummonPool[static_cast<std::size_t>(
		GameRandom::Range(0, static_cast<int>(kStandardSummonPool.size()) - 1))];
}

void RoofMarshalZombie::SummonCommandedZombies()
{
	mLastSummonCount = 0;
	mLastSummonRowMask = 0;
	mLastSummonedTypes.fill(ZombieType::NUM_ZOMBIE_TYPES);
	if (!mBoard || mBoard->mRows <= 0) return;

	std::array<int, 6> rows{};
	int availableRows = 0;
	for (int row = 0; row < mBoard->mRows
		&& availableRows < static_cast<int>(rows.size()); ++row) {
		// 召唤部队围绕督军展开，但不与它当前所在行重叠。
		if (row == mRow) continue;
		rows[static_cast<std::size_t>(availableRows++)] = row;
	}

	// 局部 Fisher-Yates：只洗出本批需要的前 N 行，保证同一批绝不重复行。
	const int requestedCount = mBodyHealth < kDesperateHealthThreshold
		? kDesperateSummonCount
		: kNormalSummonCount;
	const int summonCount = std::min(requestedCount, availableRows);
	for (int i = 0; i < summonCount; ++i) {
		const int swapIndex = GameRandom::Range(i, availableRows - 1);
		std::swap(rows[static_cast<std::size_t>(i)], rows[static_cast<std::size_t>(swapIndex)]);

		const int row = rows[static_cast<std::size_t>(i)];
		ZombieType type = RollSummonedZombieType();
		// 白名单均为屋顶可用陆地类型；此守卫防未来单项行为变化把整批召唤打断。
		if (!mBoard->CanSpawnZombieInRow(type, row)) type = ZombieType::ZOMBIE_NORMAL;
		Zombie* summoned = mBoard->CreateZombie(type, row, kCommandSpawnX);
		if (!summoned) continue;

		mLastSummonedTypes[static_cast<std::size_t>(mLastSummonCount)] = type;
		mLastSummonRowMask |= 1 << row;
		++mLastSummonCount;
	}
	++mCommandCount;
	if (mBodyHealth >= kDesperateHealthThreshold
		&& mCommandCount % kWeatherCommandCadence == 0) {
		// 周期命令只把晴/小雨提升到中雨；已有中雨或大雨保持原倒计时，不被永久续期。
		mBoard->TriggerRoofMarshalWeather(
			RainIntensity::MEDIUM, kCommandedMediumRainDuration, false);
	}
}

void RoofMarshalZombie::BeginCommandPose()
{
	mCommandPhase = CommandPhase::COMMANDING;
	mCommandPoseTimer = kCommandPoseDuration;
	PlayTrack("anim_idle", 0.0f, kCommandBlendTime);
}

void RoofMarshalZombie::BeginLaneSwitch()
{
	mLaneSwitchTimer = kLaneSwitchInterval;
	if (!mBoard || mRow < 0 || mRow >= mBoard->mRows) return;

	int candidates[2] = { -1, -1 };
	int candidateCount = 0;
	for (const int candidate : { mRow - 1, mRow + 1 }) {
		if (candidate < 0 || candidate >= mBoard->mRows
			|| !mBoard->CanSpawnZombieInRow(mZombieType, candidate)) {
			continue;
		}
		candidates[candidateCount++] = candidate;
	}
	if (candidateCount == 0) return;

	TransformComponent* transform = GetTransformComponent();
	if (!transform) return;
	Vector position = transform->GetPosition();
	const float oldTerrainY = mBoard->GetZombieSpawnY(mRow, position.x);
	const int destination = candidates[candidateCount == 1
		? 0
		: GameRandom::Range(0, candidateCount - 1)];
	const float newTerrainY = mBoard->GetZombieSpawnY(destination, position.x);
	if (oldTerrainY < 0.0f || newTerrainY < 0.0f) return;

	// 逻辑行与碰撞箱立即提交；独立视觉补偿从旧行高度平滑归零，遵守逻辑网格/美术偏移分离契约。
	mRow = destination;
	position.y = newTerrainY;
	transform->SetPosition(position);
	mLaneVisualOffsetY = oldTerrainY - newTerrainY;
	mLaneTransitionRemaining = kLaneTransitionDuration;
	++mLaneSwitchCount;
	PlayWalkAnimation(kLaneTransitionBlendTime);
}

void RoofMarshalZombie::UpdateLaneTransition(float scaledTime)
{
	if (mLaneTransitionRemaining <= 0.0f) return;
	const float previousRemaining = mLaneTransitionRemaining;
	mLaneTransitionRemaining = std::max(0.0f,
		mLaneTransitionRemaining - std::max(0.0f, scaledTime));
	if (previousRemaining > 0.0f) {
		mLaneVisualOffsetY *= mLaneTransitionRemaining / previousRemaining;
	}
	if (mLaneTransitionRemaining <= 0.0f) {
		mLaneVisualOffsetY = 0.0f;
		if (mCommandPhase != CommandPhase::COMMANDING) {
			PlayWalkAnimation(kLaneTransitionBlendTime);
		}
	}
}

bool RoofMarshalZombie::IsAllowedSummonType(ZombieType type)
{
	return ContainsZombieType(kStandardSummonPool, type)
		|| ContainsZombieType(kHighThreatSummonPool, type);
}

bool RoofMarshalZombie::IsHighThreatSummonType(ZombieType type)
{
	return ContainsZombieType(kHighThreatSummonPool, type);
}

const char* RoofMarshalZombie::GetCommandPhaseName() const
{
	return mCommandPhase == CommandPhase::COMMANDING ? "COMMANDING" : "ADVANCING";
}

int RoofMarshalZombie::GetLastSummonDistinctRowCount() const
{
	int count = 0;
	for (int mask = mLastSummonRowMask; mask != 0; mask >>= 1) {
		count += mask & 1;
	}
	return count;
}

bool RoofMarshalZombie::IsHighThreatPoolUnlocked() const
{
	return mBodyHealth < kHighThreatHealthThreshold;
}

int RoofMarshalZombie::GetCurrentSummonCount() const
{
	return mBodyHealth < kDesperateHealthThreshold
		? kDesperateSummonCount
		: kNormalSummonCount;
}

float RoofMarshalZombie::GetCurrentSummonInterval() const
{
	return mBodyHealth < kDesperateHealthThreshold
		? kDesperateSummonInterval
		: kNormalSummonInterval;
}

bool RoofMarshalZombie::IsWalkingPhase() const
{
	return mBodyHealth < kDesperateHealthThreshold;
}

void RoofMarshalZombie::SaveExtraData(nlohmann::json& j) const
{
	j["commandPhase"] = static_cast<int>(mCommandPhase);
	j["summonTimer"] = mSummonTimer;
	j["commandPoseTimer"] = mCommandPoseTimer;
	j["laneSwitchTimer"] = mLaneSwitchTimer;
	j["laneTransitionRemaining"] = mLaneTransitionRemaining;
	j["laneVisualOffsetY"] = mLaneVisualOffsetY;
	j["commandCount"] = mCommandCount;
	j["laneSwitchCount"] = mLaneSwitchCount;
	j["lastSummonCount"] = mLastSummonCount;
	j["lastSummonRowMask"] = mLastSummonRowMask;
	j["lastSummonedTypes"] = nlohmann::json::array();
	for (ZombieType type : mLastSummonedTypes) {
		j["lastSummonedTypes"].push_back(static_cast<int>(type));
	}
}

void RoofMarshalZombie::LoadExtraData(const nlohmann::json& j)
{
	const int rawPhase = j.value("commandPhase", static_cast<int>(CommandPhase::ADVANCING));
	mCommandPhase = rawPhase == static_cast<int>(CommandPhase::COMMANDING)
		? CommandPhase::COMMANDING
		: CommandPhase::ADVANCING;
	mSummonTimer = std::clamp(j.value("summonTimer", kFirstSummonDelay),
		0.0f, kNormalSummonInterval);
	mCommandPoseTimer = std::clamp(j.value("commandPoseTimer", 0.0f),
		0.0f, kCommandPoseDuration);
	mLaneSwitchTimer = std::clamp(j.value("laneSwitchTimer", kLaneSwitchInterval),
		0.0f, kLaneSwitchInterval);
	mLaneTransitionRemaining = std::clamp(j.value("laneTransitionRemaining", 0.0f),
		0.0f, kLaneTransitionDuration);
	mLaneVisualOffsetY = std::clamp(j.value("laneVisualOffsetY", 0.0f),
		-static_cast<float>(SCENE_HEIGHT), static_cast<float>(SCENE_HEIGHT));
	mCommandCount = std::max(0, j.value("commandCount", 0));
	mLaneSwitchCount = std::max(0, j.value("laneSwitchCount", 0));
	mLastSummonCount = std::clamp(j.value("lastSummonCount", 0), 0,
		static_cast<int>(mLastSummonedTypes.size()));
	const int validRowMask = mBoard && mBoard->mRows > 0
		? (1 << std::min(mBoard->mRows, 30)) - 1
		: 0;
	mLastSummonRowMask = j.value("lastSummonRowMask", 0) & validRowMask;
	mLastSummonedTypes.fill(ZombieType::NUM_ZOMBIE_TYPES);
	if (j.contains("lastSummonedTypes") && j["lastSummonedTypes"].is_array()) {
		const auto& savedTypes = j["lastSummonedTypes"];
		for (int i = 0; i < mLastSummonCount && i < static_cast<int>(savedTypes.size()); ++i) {
			const ZombieType type = static_cast<ZombieType>(savedTypes[i].get<int>());
			if (IsAllowedSummonType(type)) {
				mLastSummonedTypes[static_cast<std::size_t>(i)] = type;
			}
		}
	}

	// COMMANDING 必须有正的姿势剩余时间；濒死/损坏组合则原子恢复为可推进状态。
	if (mCommandPhase == CommandPhase::COMMANDING
		&& mCommandPoseTimer > 0.0f && mHasHead && !mIsDying) {
		if (!mIsEating) PlayTrack("anim_idle", 0.0f, 0.0f);
	}
	else {
		mCommandPhase = CommandPhase::ADVANCING;
		mCommandPoseTimer = 0.0f;
	}

	if (mBodyHealth < kDesperateHealthThreshold) {
		mLaneSwitchTimer = 0.0f;
		mLaneTransitionRemaining = 0.0f;
		mLaneVisualOffsetY = 0.0f;
	}
	else if (mLaneTransitionRemaining <= 0.0f) {
		mLaneVisualOffsetY = 0.0f;
	}
	if (!mIsEating && mCommandPhase != CommandPhase::COMMANDING) {
		PlayWalkAnimation(0.0f);
	}
}

void RoofMarshalZombie::TakePlantAshDamage(int damage)
{
	// 土豆雷传入 INT32_MAX；在统一灰烬入口先压回常规爆炸伤害，再保留植物词条缩放。
	Zombie::TakePlantAshDamage(std::min(damage, kPlantAshDamageCap));
}

bool RoofMarshalZombie::TakePlantInstantKill()
{
	// 大嘴花不进入消化状态，但这次完整咬合仍通过正式植物伤害链造成伤害。
	TakeDamage(kPlantInstantKillFallbackDamage, DamageSource::PLANT);
	return false;
}

void RoofMarshalZombie::HeadDrop()
{
	if (!mHasHead || !mAnimator) return;

	// 先读取仍可见的头轨世界锚点，再同步隐藏头、下巴、舌头和军帽轨道。
	const Vector particlePosition = GetTrackWorldPosition("anim_head1");
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("RoofMarshalHeadOff", particlePosition);
	}
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}
