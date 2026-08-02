#include "Zombie.h"
#include "ZombieCharred.h"
#include "GildedZamboniZombie.h"
#include "../Plant/Plant.h"
#include "../Plant/HypnoShroom.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"
#include "../Plant/GameDataManager.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../GameAPP.h"
#include <algorithm>
#include <climits>
#include <cmath>

namespace {
	float GAMEOVER_X = 110.0f;	// 僵尸到达此横坐标即触发游戏失败
	constexpr float kPoolFrontProbeX = 75.0f;              // 身体前侧入水探针相对世界 X 偏移，单位：像素
	constexpr float kPoolRearProbeX = 45.0f;               // 身体后侧入水探针相对世界 X 偏移，单位：像素
	constexpr float kPoolTransitionRightShiftX = 70.0f;    // 适配当前泳池图，把进出水边界向右校正的像素数
	constexpr float kPoolTransitionBlend = 0.2f;           // 进出水后恢复稳态走路轨道的混合秒数
	constexpr float kPoolClipBottomOffsetY = 0.0f;         // 水面裁剪底边相对行逻辑 Y 的偏移，单位：像素
	constexpr float kTangleKelpSinkSpeed = 100.0f;         // 原版拖沉速度，单位：像素/秒
	constexpr float kTangleKelpGrabOffsetX = -13.0f;       // 原版通用僵尸 anim_grab 附着点水平偏移，单位：像素
	constexpr float kTangleKelpGrabOffsetY = 15.0f;        // 原版通用僵尸 anim_grab 附着点垂直偏移，单位：像素
	constexpr float kTangleKelpGrabSpeed = 2.0f;           // 资源 12fps 播放为原版 24fps 的速度倍率
	constexpr float kTangleKelpGrabStartFrame = 22.0f;     // anim_grab 轨道在 Tanglekelp.reanim 中的首帧
	constexpr float kTangleKelpGrabEndFrame = 26.0f;       // anim_grab 轨道末帧；读档帧钳位避免损坏值越界
	constexpr int kMaxGoldenIceEffectStacks = 8;           // 独立黄色冰道来源计层的安全上限，防止极端调试生成导致倍率溢出
	constexpr float kEatingTargetRetentionGap = 6.0f;      // 跳跃受阻后允许僵尸隔空啃食的最大碰撞箱间隙，单位：像素
	constexpr float kHitGlowDuration = 0.1f;               // 本体与二类护盾受击白光持续时间，单位：游戏秒
	constexpr float kToxinLayerDuration = 6.0f;            // 单层毒素持续时间，单位：游戏秒
	constexpr float kToxinDamagePerSecond = 2.0f;          // 单层毒素每秒伤害；满四层为 8 DPS
	constexpr float kToxinDamageEpsilon = 0.0001f;         // 浮点取整容差，避免整点伤害因误差延迟一帧

	/** 对齐 C# AnimateChewSound：坚硬防御植物使用 ChompSoft，其他植物使用普通 Chomp。 */
	bool UsesSoftChewSound(PlantType type)
	{
		switch (type) {
		case PlantType::PLANT_WALLNUT:
		case PlantType::PLANT_TALLNUT:
		case PlantType::PLANT_GARLIC:
		case PlantType::PLANT_PUMPKINSHELL:
			return true;
		default:
			return false;
		}
	}
}

Zombie::Zombie(Board* board, ZombieType zombieType, float x, float y, int row,
	AnimationType animType, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_ZOMBIE, board,
		Vector(x, y),
		animType,
		ColliderType::BOX,
		Vector(50, 100),
		Vector(-25, -65),
		scale,
		"Zombie",
		false)
{
	mBoard = board;
	mZombieType = zombieType;
	mRow = row;
	mIsPreview = isPreview;

	mVisualOffset = GameDataManager::GetInstance().GetZombieOffset(zombieType);

	mAnimator->SetTrackVisible("_ground", false);

	if (isPreview && mAnimator->HasTrack("anim_idle"))
	{
		this->PlayTrack("anim_idle");
		return;
	}

	if (!mBoard) return;

	auto collider = GetColliderComponent();
	const float collisionY = mBoard->GetZombieCollisionY(row);
	if (collisionY >= 0.0f) {
		// 水路僵尸的 Transform 保留美术下沉，碰撞框反向抵消该差值并回到逻辑行基线。
		collider->offset.y += collisionY - y;
	}
	collider->isTrigger = true;
	collider->layerMask = CollisionLayer::ZOMBIE;
	collider->collisionMask = CollisionLayer::PLANT | CollisionLayer::BULLET | CollisionLayer::MOWER;
	collider->onTriggerEnter = [this](ColliderComponent* other) {
		this->StartEat(other);
		};
	collider->onTriggerStay = [this](ColliderComponent* other) {
		this->StartEat(other);
		};
	collider->onTriggerExit = [this](ColliderComponent* other) {
		this->StopEat(other);
		};

	mGroundTrackIndex = mAnimator->GetFirstTrackIndexByName("_ground");
}

void Zombie::SetupZombie()
{
	RegisterFrameEvents();

	mHasTongue = static_cast<bool>(GameRandom::Range(0, 1));

	if (!mAnimator->GetTracksByName("anim_tongue").empty()) {
		mAnimator->SetTrackVisible("anim_tongue", mHasTongue);
	}

	if (mIsPreview) return;

	mSpeed += GameRandom::Range(-3, 3);

	if (GameRandom::Range(0, 1) == 0)
		this->PlayTrack("anim_walk");
	else
		this->PlayTrack("anim_walk2");
}

/** 注册普通僵尸 reanim 的死亡终点与两次啃食命中帧。 */
void Zombie::RegisterFrameEvents()
{
	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(152, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(171, [this]() { this->EatTarget(); }, true);
}

void Zombie::ApplyHealthMultiplier(double multiplier)
{
	if (multiplier <= 0.0 || multiplier == 1.0) return;
	// 同一原值 × 同一倍率 → 同一舍入结果，故缩放后 current 仍等于 max。血量非负，四舍五入用 +0.5。
	// 用 double 而非 float：float 尾数仅 24 位，血量 > 2^24(≈1677万) 时整数会丢精度；double 尾数 52 位，
	// 整数精确到 ~9e15，远超 int 上限，故缩放链路上 float 是比 int 字段更先暴露的弱点（缩放仅出生时算一次，无热路径开销）。
	auto scale = [multiplier](int v) {
		double scaled = static_cast<double>(v) * multiplier + 0.5;
		// 防溢出：缩放后血量超过 INT_MAX 时 static_cast<int> 是 UB(实测得 INT_MIN)，钳到 INT_MAX。
		// (double)INT_MAX == 2147483647.0 精确可表示(2^31-1 < 2^53)，故用 >= 比较即可。
		if (scaled >= static_cast<double>(INT_MAX)) return INT_MAX;
		return static_cast<int>(scaled);
		};

	mBodyHealth = scale(mBodyHealth);
	mBodyMaxHealth = scale(mBodyMaxHealth);
	mHelmHealth = scale(mHelmHealth);
	mHelmMaxHealth = scale(mHelmMaxHealth);
	mShieldHealth = scale(mShieldHealth);
	mShieldMaxHealth = scale(mShieldMaxHealth);
	ApplyExtraHealthMultiplier(multiplier);
}

void Zombie::SaveProtectedData(nlohmann::json& j) const {
	j["isMindControlled"] = mIsMindControlled;
	j["freeHitsRemaining"] = mFreeHitsRemaining;
	j["isEating"] = mIsEating;
	j["eatPlantID"] = mEatPlantID;
	j["hasHead"] = mHasHead;
	j["hasArm"] = mHasArm;
	j["hasTongue"] = mHasTongue;
	j["isDying"] = mIsDying;
	j["inPool"] = mInPool;
	j["speed"] = mSpeed;
	j["cooldownTimer"] = mCooldownTimer;
	j["frozenTimer"] = mFrozenTimer;
	j["toxinLayerTimers"] = mToxinLayerTimers;
	j["toxinDamageRemainder"] = mToxinDamageRemainder;
	j["dyingTimer"] = mDyingTimer;
	j["tangleKelpPlantID"] = mTangleKelpPlantID;
	j["draggedUnderByTangleKelp"] = mDraggedUnderByTangleKelp;
	j["tangleKelpSinkOffset"] = mTangleKelpSinkOffset;
	j["tangleKelpGrabFrame"] = GetTangleKelpGrabFrame();
	j["mistFuelReward"] = mMistFuelReward;
	j["mistFuelRewardClaimed"] = mMistFuelRewardClaimed;
}

void Zombie::LoadProtectedData(const nlohmann::json& j) {
	mIsMindControlled = j.value("isMindControlled", false);
	mFreeHitsRemaining = j.value("freeHitsRemaining", 0);   // 旧档缺字段→0
	mIsEating = j.value("isEating", false);
	mEatPlantID = j.value("eatPlantID", NULL_PLANT_ID);
	mHasHead = j.value("hasHead", true);
	mHasArm = j.value("hasArm", true);
	mHasTongue = j.value("hasTongue", false);
	mIsDying = j.value("isDying", false);
	mInPool = j.value("inPool", false);
	mSpeed = j.value("speed", 10.0f);

	// 旧档把品种能力倍率放在根字段 extraSpeed；只有仍需实例随机值的派生类消费它。
	// 固定倍率和状态倍率均由虚函数直接派生，新档不再写该字段。
	if (j.contains("extraSpeed")) {
		RestoreLegacyAbilityAnimSpeedMultiplier(j.value("extraSpeed", 1.0f));
	}
	float cooldown = j.value("cooldownTimer", 0.0f);
	if (cooldown > 0.0f) {
		this->SetCooldown(cooldown);
	}
	else if (mAnimator) {
		UpdateAnimSpeed();   // 无减速也要恢复僵尸基准 × 当前雨势倍率
	}

	// 冻结还原：必须在减速恢复之后——UpdateAnimSpeed 里冻结优先，把 extra 覆盖回 0（停格）。
	mFrozenTimer = j.value("frozenTimer", 0.0f);
	if (mFrozenTimer > 0.0f && mAnimator) {
		UpdateAnimSpeed();
	}

	mToxinLayerTimers.fill(0.0f);
	if (const auto it = j.find("toxinLayerTimers"); it != j.end() && it->is_array()) {
		const std::size_t count = std::min(it->size(), mToxinLayerTimers.size());
		for (std::size_t index = 0; index < count; ++index) {
			mToxinLayerTimers[index] = std::clamp(
				(*it)[index].get<float>(), 0.0f, kToxinLayerDuration);
		}
	}
	mToxinDamageRemainder = std::clamp(
		j.value("toxinDamageRemainder", 0.0f), 0.0f, 0.9999f);
	// 魅惑状态不允许携带任何敌对植物的延迟伤害；旧档也在这里归一化。
	if (mIsMindControlled) {
		mCooldownTimer = 0.0f;
		mFrozenTimer = 0.0f;
		mToxinLayerTimers.fill(0.0f);
		mToxinDamageRemainder = 0.0f;
		UpdateAnimSpeed();
		ApplyCharmEffects();
	}
	else {
		UpdateStatusOverlay();
	}

	// 如果播放死亡动画，禁用碰撞箱（判空与 Die/预览路径一致：预览僵尸已移除碰撞箱、mCollider=null）
	if (mIsDying && mCollider) {
		mCollider->mEnabled = false;
	}

	mDyingTimer = j.value("dyingTimer", 0.0f);
	mTangleKelpPlantID = j.value("tangleKelpPlantID", NULL_PLANT_ID);
	mDraggedUnderByTangleKelp = j.value("draggedUnderByTangleKelp", false);
	mTangleKelpSinkOffset = std::max(0.0f, j.value("tangleKelpSinkOffset", 0.0f));
	mMistFuelReward = std::max(0.0f, j.value("mistFuelReward", 0.0f));
	mMistFuelRewardClaimed = j.value("mistFuelRewardClaimed", false);
	if (mTangleKelpPlantID != NULL_PLANT_ID) {
		CreateTangleKelpGrabAnimators(
			j.value("tangleKelpGrabFrame", kTangleKelpGrabStartFrame));
	}
	if (mDyingTimer >= 10.0f) {
		this->Die();
	}
}

void Zombie::ZombieItemUpdate() const
{
	if (!mHasArm) {
		mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
		mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
		mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
			GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	}
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_tongue", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
	mAnimator->SetTrackVisible("anim_tongue", mHasTongue);
	UpdatePoolVisualState();
}

void Zombie::Charred()
{
	GameObjectManager::GetInstance().CreateGameObjectImmediate<ZombieCharred>
		(LAYER_GAME_ZOMBIE, ObjectType::OBJECT_ZOMBIE, mBoard, this->GetVisualPosition(), AnimationType::ANIM_ZOMBIE_CHARRED);
	Die();
}

void Zombie::TakePlantAshDamage(int damage)
{
	if (damage <= 0 || !mBoard) return;

	// 化灰阈值必须与 TakeDamage 的最终词条倍率一致；这里只预测是否走表现，真正扣血仍由
	// TakeDamage 单点缩放，避免植物增伤被重复应用。
	const int scaledDamage = mBoard->GetPerkManager().ScaleTotalDamageToZombie(damage);
	if (CanBeCharred() && mBodyHealth <= scaledDamage) {
		Charred();
		return;
	}
	TakeDamage(damage, DamageSource::PLANT_ASH);
}

bool Zombie::TakePlantInstantKill()
{
	Die();
	return true;
}

void Zombie::Start()
{
	AnimatedObject::Start();
	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	mPoolShadow = shadowcomponent;
	shadowcomponent->SetDrawOrder(-80);
	if (this->mIsPreview) {
		RemoveComponent<ColliderComponent>();
		mCollider = nullptr;  // 缓存的裸指针随之失效，显式置空
	}
	SetAnimationSpeed(GameRandom::Range(1.1f, 1.4f));
	SetupZombie();
	ConfigureShieldHitGlowTrack();
	mGoldenIceEffectStacks = ComputeGoldenIceEffectStacks();
	// 子类虚函数提供品种能力倍率；最后统一叠加减速、冻结、雨势和场地效果，且跨 PlayTrack 存活。
	if (!mIsPreview) UpdateAnimSpeed();
	// 直接生成在水域内部时首帧就应进入水中，避免等待移动一帧后才裁剪。
	UpdatePoolState();
}

void Zombie::CheckWin() const
{
	if (mBoard && mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0)
	{
		WinGame();
	}
}

void Zombie::WinGame() const
{
	if (mBoard->mIsSurvival)
	{
		mBoard->OnSurvivalRoundClear();   // 生存模式：进入下一轮，不出奖杯
	}	
	else
	{
		Vector trophyPos = GetPosition();
		if (trophyPos.x > SCENE_WIDTH - 10)
		{
			trophyPos.x = SCENE_WIDTH - 10;
		}
		else if (trophyPos.x < 20.0f)
		{
			trophyPos.x = 20.0f;
		}
		mBoard->CreateTrophy(trophyPos);
	}
}

void Zombie::Update()
{
	AnimatedObject::Update();
	UpdateShieldHitGlow();
	if (mTangleKelpGrabBack) mTangleKelpGrabBack->Update();
	if (mTangleKelpGrabFront) mTangleKelpGrabFront->Update();
	if (!mIsPreview) {
		float deltaTime = DeltaTime::GetDeltaTime();
		auto* transform = this->GetTransformComponent();

		if (!transform || !mBoard) return;

		// 毒素不受减速、冻结、啃食和水草早退影响，因此在所有行为状态分支之前结算。
		UpdateToxin(deltaTime);
		if (!IsActive()) return;

		if (mTangleKelpPlantID != NULL_PLANT_ID
			&& !mBoard->mEntityManager.GetPlant(mTangleKelpPlantID)) {
			ClearOrphanedTangleKelpGrab();
		}
		if (mDraggedUnderByTangleKelp) {
			mTangleKelpSinkOffset += kTangleKelpSinkSpeed * deltaTime;
		}

		if (mIsDying)
		{
			// 冻结兜底解除：任何转入死亡的路径都不得停格——死亡动画靠帧事件 Die()，停格即卡尸
			if (mFrozenTimer > 0.0f) ClearFrozen();
			mDyingTimer += deltaTime;
			if (GetCurrentTrackName() != GetDeathTrackName() && !mDbgAnomalyLogged) {
				mDbgAnomalyLogged = true;
			}
			if (mDyingTimer >= 10.0f)
			{
				LOG_WARN("DBG") << "WATCHDOG force-die type=" << static_cast<int>(mZombieType)
					<< " track=" << GetCurrentTrackName()
					<< " frame=" << GetCurrentFrame()
					<< " target=" << GetTargetTrack()
					<< " playState=" << static_cast<int>(GetPlayingState())
					<< " hasHead=" << mHasHead << " isEating=" << mIsEating;
				this->Die();
				return;
			}
		}

		// —— 减速滴答（用真实 deltaTime，保证以真实秒数倒计时） ——
		if (mCooldownTimer > 0.0f)
		{
			mCooldownTimer -= deltaTime;
			if (mCooldownTimer <= 0.0f)
			{
				mCooldownTimer = 0.0f;
				UpdateAnimSpeed();
				UpdateStatusOverlay();
			}
		}

		// —— 冻结滴答（同样用真实 deltaTime：定身时长不被减速自身拖慢） ——
		if (mFrozenTimer > 0.0f)
		{
			mFrozenTimer -= deltaTime;
			if (mFrozenTimer <= 0.0f)
			{
				ClearFrozen();   // 解冻：减速尾巴未尽则回 0.6x（蓝光归尾巴管），否则回常速+褪色
			}
		}

		// —— 减速时 50% 缩放僵尸内部逻辑 deltaTime ——
		const float slowMul = (mCooldownTimer > 0.0f) ? 0.5f : 1.0f;
		const float scaledDelta = deltaTime * slowMul;

		mCheckPositionTimer += deltaTime;
		if (mCheckPositionTimer >= 1.0f)
		{
			mCheckPositionTimer = 0.0f;
			Vector position = transform->GetPosition();
			if (position.x <= GAMEOVER_X && CanTriggerGameOver()
				&& mBoard->mBoardState == BoardState::GAME)
			{
				mBoard->GameOver();
			}
			if (position.x > static_cast<float>(SCENE_WIDTH + 65) || position.x < -20.0f)
			{
				this->Die();
			}
		}

		// 阵风是空气施加的独立位移：在冻结/啃食的早退前结算，使碰撞箱随 Transform 同帧移动。
		// 水草关系同时充当水底锚点，束缚期间不允许阵风改变僵尸的位置。
		if (mTangleKelpPlantID == NULL_PLANT_ID) {
			ApplyTyphoonGustDrift(deltaTime, transform);
		}
		// 海豚/撑杆被高坚果挡下后会在碰撞箱外手动开吃，没有碰撞对可产生 onTriggerExit。
		// 因而每帧都以实体生命、顶层身份和实际间距复核一次，阵风吹离或目标死亡时立即收尾。
		if (!mIsDying && mIsEating && mEatPlantID != NULL_PLANT_ID
			&& !IsCurrentPlantEatingTargetValid()) {
			StopEatingInvalidPlantTarget(0.2f);
		}
		// 入水状态是通用介质状态：冻结或啃食期间也要跟随阵风后的实际位置更新。
		if (!mIsDying) UpdatePoolState();
		// 黄色冰道可能在本帧延伸、消失，或被阵风跨越；在冻结/啃食早退前刷新速度场边沿。

		mCheckGoldenIceTimer += deltaTime;
		if (mCheckGoldenIceTimer >= 0.4f)
		{
			mCheckGoldenIceTimer = 0.0f;
			RefreshGoldenIceSpeedState();
		}

		if (!mHasHead)
		{
			// 掉头后本体血量按真实时间流失直至归零（无头僵尸流血而亡）。
			// 流血速率 = 本体血量上限的 10%/秒（纯百分比扣血）。掉头发生在血量降到 ≈上限/3，再流到
			// 死亡阈值 35，故掉头到倒地耗时 ≈ (1/3)/10% ≈ 3 秒，且基本与僵尸血量大小无关：无论普通僵尸
			// 还是后期被生存高轮次/词条（ApplyHealthMultiplier）放大数十倍的僵尸，流血倒地都≈ 2~3.3 秒。
			// 不设固定 HP/秒地板：固定速率在放大后的血量上会让残血需几十秒才流尽（主人反馈“后期减血
			// 太慢”），对普通僵尸又显得过快；纯百分比两头都自洽。下面 1.0f 仅为 mBodyMaxHealth 极小时
			// 的除零兜底（bleedPerSec 当除数），正常僵尸（上限≥270）永不触发、非行为地板。
			// 用未减速的 deltaTime（非 scaledDelta）：流血而亡是确定性死亡机制，速率必须只与
			// 真实时间挂钩——既与帧率无关（高刷不会掉得更快），也不被冰冻减速拖慢（否则高血量
			// 僵尸被冰住时迟迟不死、继续逼近房子）。
			// 累加器保留亚阈值余量、并在长帧（低帧率 / set_timescale 快进）一次补扣多点，速率才恒定。
			float bleedPerSec = mBodyMaxHealth * 0.10f;
			if (bleedPerSec < 1.0f) bleedPerSec = 1.0f;   // 仅除零兜底，非行为地板

			mSubHealthTimer += deltaTime;
			float bleedAmount = mSubHealthTimer * bleedPerSec;
			if (bleedAmount >= 1.0f)
			{
				// 单次最多扣到当前血量：防一次扣过头，也避免 mBodyMaxHealth 极大 + 长帧时 (int) 转换溢出 UB。
				int dmg = (bleedAmount >= static_cast<float>(mBodyHealth))
					? mBodyHealth
					: static_cast<int>(bleedAmount);
				mSubHealthTimer -= dmg / bleedPerSec;   // 仅扣掉已结算的时间，保留亚阈值余量
				mBodyHealth -= dmg;             // 钳在 0：避免跌成负数污染 Board 总血量统计与刷波阈值
				if (mBodyHealth < 0) mBodyHealth = 0;
			}

			if (mBodyHealth <= 35)
			{
				if (!mIsDying)
				{
					if (!ShouldPlayDeathAnimation()) {
						Die();
						return;
					}
					// 冻着也要能倒：先解停格再播死亡动画（帧事件 Die 依赖动画前进）
					if (mFrozenTimer > 0.0f) ClearFrozen();
					// 死亡轨道开始前立即结束攻击，避免重复啃食帧事件继续伤害目标，
					// 也避免植物的 eaterCount 一直等到死亡动画末帧才归零。
					if (mIsEating) {
						if (mEatPlantID != NULL_PLANT_ID && mBoard) {
							if (Plant* plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
								plant && plant->mEaterCount > 0) {
								--plant->mEaterCount;
							}
						}
						mIsEating = false;
						mEatPlantID = NULL_PLANT_ID;
						mEatZombieID = NULL_ZOMBIE_ID;
						OnStopEating();
					}
					PlayTrack(GetDeathTrackName(), 1.3f, 0.3f);
					if (mCollider) mCollider->mEnabled = false;
					mIsDying = true;
				}
				return;
			}
		}

		// 水草束缚只停移动、碰撞行为和品种逻辑；上方的动画、状态计时与无头流血仍继续。
		if (mTangleKelpPlantID != NULL_PLANT_ID) return;

		// 冻结定身：移动/啃食推进/子类逻辑全停（上方的无头流血、减速与冻结滴答照走）。
		// 啃食帧事件因动画停格（extra=0）自然不触发，mIsEating 状态保留，解冻续啃。
		if (mFrozenTimer > 0.0f) return;

		if (mIsEating) return;

		ZombieMove(scaledDelta, transform);
		ZombieUpdate(scaledDelta);
	}
}

/**
 * 将 Board 给出的有符号阵风速度直接叠加到世界坐标。吹向前线时在出生边界内钳位，
 * 避免阵风把刚生成的僵尸推过现有清理线；吹向房屋则保留其真实危险性。
 */
void Zombie::ApplyTyphoonGustDrift(float deltaTime, TransformComponent* transform)
{
	if (!transform || !mBoard || mIsDying || deltaTime <= 0.0f) return;
	const float velocity = mBoard->GetZombieGustDriftVelocity();
	if (velocity == 0.0f) return;
	float displacement = velocity * deltaTime;
	if (velocity > 0.0f) {
		const float limit = mBoard->GetZombieGustFrontLimit();
		const float x = transform->GetPosition().x;
		if (x >= limit) return;
		displacement = std::min(displacement, limit - x);
	}
	transform->Translate(displacement, 0.0f);
}

/** 双探针同时落入泳池才切入水中，避免僵尸横跨池沿时来回闪动。 */
void Zombie::UpdatePoolState()
{
	if (!mBoard || mIsPreview || mRow < 0) return;
	if (!CanUseGroundPoolState()) {
		if (mInPool) {
			mInPool = false;
			UpdatePoolVisualState();
		}
		return;
	}

	const float x = GetPosition().x;
	const bool shouldBeInPool =
		mBoard->IsPoolWorldPosition(mRow,
			x + kPoolFrontProbeX - kPoolTransitionRightShiftX) &&
		mBoard->IsPoolWorldPosition(mRow,
			x + kPoolRearProbeX - kPoolTransitionRightShiftX);
	if (shouldBeInPool == mInPool) return;

	mInPool = shouldBeInPool;
	UpdatePoolVisualState();
	// 啃食轨道结束时会经 ResumeWalkAfterEat 读取最新介质；此处不抢占正在播放的啃食。
	if (!mIsEating && !mIsDying) {
		PlayWalkAnimation(kPoolTransitionBlend);
	}
}

/** 水中不绘制陆地投影；实际水面裁剪在 Draw 内压入 Graphics 裁剪栈。 */
void Zombie::UpdatePoolVisualState() const
{
	if (mPoolShadow) {
		mPoolShadow->SetVisible(!mInPool);
	}
}

void Zombie::ZombieMove(float scaledDelta, TransformComponent* transform)
{
	float speed = 0.0f;
	// 尝试从 _ground 轨道获取速度
	if (mAnimator) {
		// GetTrackVelocity 内部已乘 animator mSpeed，减速时 SetSpeed(0.5) 已令其自动减半
		//  但 this->mSpeed 是独立的固定项，需单独在减速时乘 0.5 但是deltaTime又变小了，所以不用

		speed = (mGroundTrackIndex >= 0
			? mAnimator->GetTrackVelocity(mGroundTrackIndex)
			: mAnimator->GetTrackVelocity("_ground")) * mSpeed;
		// 台风只缩放水平位移，不改 Animator extra：雨天、减速和冻结仍由 UpdateAnimSpeed
		// 统一组合，啃食、召唤与其他技能不会被顺风误加速。魅惑僵尸按实际向右移动判定顺逆风。
		if (mBoard) {
			speed *= AmplifySpeedMultiplierForGoldenIce(
				mBoard->GetZombieWindMoveMultiplier(IsMovingRight()));
		}
		if (IsMovingRight())
		{
			transform->Translate(speed * scaledDelta, 0);
		}
		else
		{
			transform->Translate(-speed * scaledDelta, 0);
		}
	}
}

void Zombie::SetCooldown(float timer, bool bypassShield)
{
	if (!mAnimator || (mShieldType != ShieldType::SHIELDTYPE_NONE && !bypassShield)) return;

	// 已在减速中则取 max，避免短射缩短减速
	mCooldownTimer = std::max(mCooldownTimer, timer);
	UpdateAnimSpeed();
	UpdateStatusOverlay();
}

void Zombie::UpdateAnimSpeed()
{
	if (!mAnimator) return;
	if (mFrozenTimer > 0.0f)
	{
		mAnimator->SetExtraSpeedMultiplier(0.0f);   // 冻结停格（同 WallNut 被啃暂停：状态层，不动 base）
		return;
	}
	const float rainMultiplier = mBoard ? mBoard->GetZombieRainSpeedMultiplier() : 1.0f;
	mAnimator->SetExtraSpeedMultiplier(
		GetAmplifiedAbilitySpeedMultiplier()
		* AmplifySpeedMultiplierForGoldenIce(
			mCooldownTimer > 0.0f ? GetSlowAnimFactor() : 1.0f)
		* AmplifySpeedMultiplierForGoldenIce(rainMultiplier));
}

float Zombie::GetAmplifiedAbilitySpeedMultiplier() const
{
	return AmplifySpeedMultiplierForGoldenIce(GetAbilityAnimSpeedMultiplier());
}

float Zombie::AmplifySpeedMultiplierForGoldenIce(float multiplier) const
{
	float amplified = std::max(0.0f, multiplier);
	for (int stack = 0; stack < mGoldenIceEffectStacks; ++stack) {
		// 中性倍率不变；每个来源让加速项直接乘二、减速项直接减半。
		if (amplified > 1.0f) amplified *= 2.0f;
		else if (amplified < 1.0f) amplified *= 0.5f;
	}
	return amplified;
}

int Zombie::ComputeGoldenIceEffectStacks() const
{
	if (!mBoard || mIsPreview || mIsDead) return IsAlwaysAffectedByGoldenIce() ? 1 : 0;

	int stacks = 0;
	const bool targetIsGilded = dynamic_cast<const GildedZamboniZombie*>(this) != nullptr;
	const int firstSourceRow = std::max(0, mRow - 1);
	const int lastSourceRow = std::min(mBoard->mRows - 1, mRow + 1);
	mBoard->mEntityManager.ForEachGoldenIceSource(
		[&](GildedZamboniZombie* gilded) {
			if (!gilded || gilded->mRow < firstSourceRow
				|| gilded->mRow > lastSourceRow
				|| !gilded->IsActive() || gilded->IsDying()) return;
			if (gilded->ProvidesGoldenIceEffectAt(
				mRow, GetPosition().x, targetIsGilded)) {
				++stacks;
			}
		});

	// 车辆死亡后黄色冰道仍保留 35 秒；失去来源身份后继续作为一层非叠加速度场。
	if (stacks == 0 && (IsAlwaysAffectedByGoldenIce()
		|| mBoard->IsGoldenIceAtWorld(mRow, GetPosition().x))) {
		stacks = 1;
	}
	return std::min(stacks, kMaxGoldenIceEffectStacks);
}

void Zombie::RefreshGoldenIceSpeedState()
{
	if (!mBoard || mIsPreview || mIsDead) return;
	const int stacks = ComputeGoldenIceEffectStacks();
	if (stacks == mGoldenIceEffectStacks) return;
	mGoldenIceEffectStacks = stacks;
	UpdateAnimSpeed();
}

bool Zombie::CanBeChilled() const
{
	// 魅惑免疫（原版 CanBeChilled 排除 mMindControlled）；预览/垂死/已死不结算
	return !mIsPreview && !mIsDead && !mIsDying && !mIsMindControlled;
}

bool Zombie::StartFrozen()
{
	if (!CanBeChilled()) return false;

	const bool wasSlowedOrFrozen = (mCooldownTimer > 0.0f || mFrozenTimer > 0.0f);
	SetCooldown(20.0f);               // 减速尾巴（原版 ApplyChill 2000cs）；持盾守卫在其内部
	if (!CanBeFrozen()) return false; // 撑杆跳跃中等：只吃减速不定身

	mFrozenTimer = wasSlowedOrFrozen
		? GameRandom::Range(3.0f, 4.0f)    // 已减速/已冻再冻缩短（原版 300~400cs，防连放无限定身）
		: GameRandom::Range(4.0f, 6.0f);   // 首冻（原版 400~600cs）
	UpdateAnimSpeed();
	UpdateStatusOverlay();

	// 附带 20 点伤害（原版 HitIceTrap 固定值，在免疫判定之后——魅惑/跳跃中撑杆不掉血）。
	// 走 TakeDamage 正常链（护盾→头盔→本体）；先停格再结算：报纸狂暴等
	// 连锁里的 UpdateAnimSpeed 看到冻结态，不会把停格顶掉。
	TakeDamage(20, DamageSource::PLANT);
	return true;
}

void Zombie::ClearFrozen()
{
	mFrozenTimer = 0.0f;
	UpdateAnimSpeed();
	UpdateStatusOverlay();
}

void Zombie::RemoveColdEffects()
{
	if (mCooldownTimer <= 0.0f && mFrozenTimer <= 0.0f) return;

	mCooldownTimer = 0.0f;
	mFrozenTimer = 0.0f;
	UpdateAnimSpeed();
	UpdateStatusOverlay();
}

int Zombie::GetToxinLayerCount() const
{
	return static_cast<int>(std::count_if(
		mToxinLayerTimers.begin(), mToxinLayerTimers.end(),
		[](float timer) { return timer > 0.0f; }));
}

float Zombie::GetToxinMaxRemaining() const
{
	return *std::max_element(mToxinLayerTimers.begin(), mToxinLayerTimers.end());
}

bool Zombie::ApplyToxinStack()
{
	if (mIsPreview || mIsDead || mIsDying || mIsMindControlled || !IsActive()) return false;

	auto slot = std::find_if(mToxinLayerTimers.begin(), mToxinLayerTimers.end(),
		[](float timer) { return timer <= 0.0f; });
	if (slot == mToxinLayerTimers.end()) {
		// 满层后只刷新最早到期的一层，所有毒囊射手共同受此目标级上限约束。
		slot = std::min_element(mToxinLayerTimers.begin(), mToxinLayerTimers.end());
	}
	*slot = kToxinLayerDuration;
	UpdateStatusOverlay();
	return true;
}

void Zombie::ClearToxin()
{
	mToxinLayerTimers.fill(0.0f);
	mToxinDamageRemainder = 0.0f;
	UpdateStatusOverlay();
}

void Zombie::UpdateToxin(float deltaTime)
{
	if (GetToxinLayerCount() == 0) return;
	if (mIsDead || mIsDying || mIsMindControlled) {
		ClearToxin();
		return;
	}
	if (deltaTime <= 0.0f) return;

	float activeLayerSeconds = 0.0f;
	for (float& timer : mToxinLayerTimers) {
		if (timer <= 0.0f) continue;
		activeLayerSeconds += std::min(timer, deltaTime);
		timer = std::max(0.0f, timer - deltaTime);
	}

	mToxinDamageRemainder += activeLayerSeconds * kToxinDamagePerSecond;
	const int damage = static_cast<int>(
		std::floor(mToxinDamageRemainder + kToxinDamageEpsilon));
	if (damage > 0) {
		mToxinDamageRemainder = std::max(0.0f, mToxinDamageRemainder - damage);
		// 速度 0 表示从正面命中当前防护层；毒伤永不以背击规则绕过门板。
		// 每点分别走正式链，避免高倍速长帧把多次毒跳合并成一次免伤/词条结算。
		for (int point = 0; point < damage; ++point) {
			TakeProjectileDamage(1, DamageSource::PLANT, 0.0f);
			if (!IsActive()) return;
		}
	}
	if (GetToxinLayerCount() == 0) {
		mToxinDamageRemainder = 0.0f;
	}
	UpdateStatusOverlay();
}

void Zombie::UpdateStatusOverlay()
{
	if (!mAnimator) return;
	if (mIsMindControlled) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(255, 64, 64, 160);
	}
	else if (mFrozenTimer > 0.0f || mCooldownTimer > 0.0f) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}
	else if (GetToxinLayerCount() > 0) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(175, 70, 215, 180);
	}
	else {
		mAnimator->EnableOverlayEffect(false);
	}
}

bool Zombie::IsFireResistant() const
{
	return mZombieType == ZombieType::ZOMBIE_ZAMBONI
		|| mZombieType == ZombieType::ZOMBIE_GILDED_ZAMBONI
		|| mShieldType == ShieldType::SHIELDTYPE_DOOR
		|| mShieldType == ShieldType::SHIELDTYPE_LADDER;
}

bool Zombie::ShouldProjectileBypassShield(float velocityX) const
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE || velocityX == 0.0f) return false;

	// 僵尸面向与自主移动方向一致；同向子弹是在其身后追上，未经过正面的二类护盾。
	return (velocityX > 0.0f) == IsMovingRight();
}

void Zombie::TakeProjectileDamage(
	int damage, DamageSource source, float velocityX, bool penetrateShield,
	bool discardShieldOverflow)
{
	TakeDamage(damage, source, penetrateShield, discardShieldOverflow,
		ShouldProjectileBypassShield(velocityX));
}

void Zombie::ApplyCharmEffects()
{
	// 碰撞：换 CHARMED 层 → 分桶自动落入 seeker 桶（mRowOthers），二分搜同行僵尸；
	// collisionMask 只含 ZOMBIE：不含 PLANT（不误啃植物）、不含 BULLET（子弹判不中）。
	// 植物/子弹/小推车的精确掩码与 CHARMED 两向都不相交 → 它们自动无视魅惑僵尸（CanCollide 是双向 OR）。
	if (mCollider) {
		mCollider->layerMask = CollisionLayer::CHARMED;
		mCollider->collisionMask = CollisionLayer::ZOMBIE;
	}
	// 视觉：魅惑红光优先于所有其他共享 overlay 的状态。
	if (mAnimator) {
		UpdateStatusOverlay();
		mAnimator->SetFlipX(true, 48.0f);   // 支点≈身体中线（动画局部坐标），截图目验后微调
	}
}

void Zombie::PlayWalkAnimation(float blendTime)
{
	// 有通用 swim 轨道的僵尸入水后自动使用；其余品种保留各自陆地走路轨道并由水面统一裁剪。
	if (mInPool && mAnimator && mAnimator->HasTrack("anim_swim")) {
		PlayTrack("anim_swim", 0.0f, blendTime);
		return;
	}
	// 基类陆地稳态走路：anim_walk2、clip 清零（回落 base 走速）。无此轨道的子类覆写本函数。
	PlayTrack("anim_walk2", 0.0f, blendTime);
}

void Zombie::StartMindControlled()
{
	if (mIsMindControlled || mIsDying || !CanBeCharmed()) return;

	// 正在啃植物：先解除（平衡 mEaterCount）。掩码换掉后与植物不再成对，onTriggerExit 不会补触发。
	if (mIsEating && mEatPlantID != NULL_PLANT_ID && mBoard) {
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			plant->mEaterCount--;
		}
		mIsEating = false;
		mEatPlantID = NULL_PLANT_ID;
		ResumeWalkAfterEat(0.2f);
	}

	// 正在啃僵尸也同样解除：魅惑瞬间双方变同阵营，残留一帧可能误伤（帧事件）
	if (mIsEating && mEatZombieID != NULL_ZOMBIE_ID) {
		mIsEating = false;
		mEatZombieID = NULL_ZOMBIE_ID;
		ResumeWalkAfterEat(0.2f);
	}

	// 清减速+冻结：把 overlay 让给红光；魅惑免疫寒冰效果（原版），动画立即恢复
	if (mCooldownTimer > 0.0f || mFrozenTimer > 0.0f) {
		mCooldownTimer = 0.0f;
		mFrozenTimer = 0.0f;
		UpdateAnimSpeed();
	}
	ClearToxin();

	// 如果是最后一波的最后一个僵尸，魅惑后就不会再有僵尸了，直接死亡
	if (mBoard && mBoard->mCurrentWave == mBoard->mMaxWave && mBoard->mZombieNumber == 1)
	{
		this->Die();
	}

	mIsMindControlled = true;
	ApplyCharmEffects();
}

int Zombie::TakeShieldDamage(int damage)
{
	if (mShieldHealth <= 0)
	{
		mShieldType = ShieldType::SHIELDTYPE_NONE;
		return damage;
	}
	mShieldHealth -= damage;
	CheckShieldImage();
	if (mShieldHealth <= 0)
	{
		int overflow = -mShieldHealth; // 剩余伤害
		mShieldHealth = 0;
		ShieldDrop();
		return overflow;
	}
	return 0;
}

const char* Zombie::GetShieldGlowTrackName(ShieldType shieldType) const
{
	switch (shieldType) {
	case ShieldType::SHIELDTYPE_DOOR:
		return "anim_screendoor";
	case ShieldType::SHIELDTYPE_NEWSPAPER:
		return "Zombie_paper_paper";
	case ShieldType::SHIELDTYPE_LADDER:
		return "Zombie_ladder_1";
	default:
		return nullptr;
	}
}

void Zombie::ConfigureShieldHitGlowTrack()
{
	if (!mAnimator) return;
	if (const char* trackName = GetShieldGlowTrackName(mShieldType)) {
		// 原版把二类护盾放在独立 RENDER_GROUP_SHIELD；本项目用轨道覆盖保留同一高亮语义。
		mAnimator->SetTrackGlowOverride(trackName, false);
	}
}

void Zombie::StartShieldHitGlow(ShieldType shieldType)
{
	if (shieldType == ShieldType::SHIELDTYPE_NONE) return;
	if (mAnimator && mShieldHitGlowType != ShieldType::SHIELDTYPE_NONE
		&& mShieldHitGlowType != shieldType) {
		if (const char* previousTrack = GetShieldGlowTrackName(mShieldHitGlowType)) {
			mAnimator->SetTrackGlowOverride(previousTrack, false);
		}
	}
	mShieldHitGlowType = shieldType;
	mShieldHitGlowTimer = kHitGlowDuration;
	if (mAnimator) {
		if (const char* trackName = GetShieldGlowTrackName(shieldType)) {
			mAnimator->SetTrackGlowOverride(trackName, true);
		}
	}
}

void Zombie::UpdateShieldHitGlow()
{
	if (mShieldHitGlowTimer <= 0.0f) return;
	mShieldHitGlowTimer -= DeltaTime::GetDeltaTime();
	if (mShieldHitGlowTimer > 0.0f) return;

	mShieldHitGlowTimer = 0.0f;
	if (mAnimator) {
		if (const char* trackName = GetShieldGlowTrackName(mShieldHitGlowType)) {
			mAnimator->SetTrackGlowOverride(trackName, false);
		}
	}
	mShieldHitGlowType = ShieldType::SHIELDTYPE_NONE;
}

bool Zombie::IsShieldTrackGlowing() const
{
	if (!mAnimator) return false;
	const ShieldType glowType = mShieldHitGlowType != ShieldType::SHIELDTYPE_NONE
		? mShieldHitGlowType : mShieldType;
	const char* trackName = GetShieldGlowTrackName(glowType);
	return trackName && mAnimator->GetTrackVisible(trackName)
		&& mAnimator->GetTrackGlowEffectEnabled(trackName);
}

int Zombie::TakeHelmDamage(int damage)
{
	if (mHelmHealth <= 0)
	{
		mHelmType = HelmType::HELMTYPE_NONE;
		return damage;
	}
	mHelmHealth -= damage;
	CheckHelmImage();
	if (mHelmHealth <= 0)
	{
		int overflow = -mHelmHealth;
		mHelmHealth = 0;
		HelmDrop();
		return overflow;
	}
	return 0;
}

void Zombie::TakeBodyDamage(int damage)
{
	mBodyHealth -= damage;
	if (mBodyHealth < 0)
		mBodyHealth = 0;

	// 先乘后除：用 64 位算中间量，避免 mBodyMaxHealth 极大时 *2 在 int 内溢出（约 >10.7 亿即翻负）。
	if (mNeedDropArm && mHasArm && mBodyHealth <= static_cast<int64_t>(mBodyMaxHealth) * 2 / 3)
	{
		ArmDrop();
		mHasArm = false;
	}
	if (mNeedDropHead && mHasHead && mBodyHealth <= mBodyMaxHealth / 3)
	{
		HeadDrop();
		mHasHead = false;
	}
}

void Zombie::TakeDamage(
	int damage, DamageSource source, bool penetrateShield, bool discardShieldOverflow,
	bool bypassShield)
{
	if (damage <= 0 || !mBoard) return;

	// 词条②：僵尸前 N 次免伤（生存专用）。出生时由词条层数设定 mFreeHitsRemaining。
	// 提前 return：完全吸收且不触发受击白光，0 伤害不应闪。
	if (mFreeHitsRemaining > 0) { --mFreeHitsRemaining; return; }

	// 植物增伤只放大植物来源（普通/灰烬）；僵尸免伤则对所有实际承伤生效。两者均在 0 层返回单位元。
	if (source == DamageSource::PLANT || source == DamageSource::PLANT_ASH) {
		damage = mBoard->GetPerkManager().ScalePlantDamage(damage);
	}
	damage = mBoard->GetPerkManager().ScaleDamageToZombie(damage);
	damage = AdjustIncomingDamage(damage, source, penetrateShield, bypassShield);
	if (damage <= 0) return;

	int remainingDamage = TakeExtraProtectionDamage(damage, source);
	// 原版飞行额外生命与头盔/本体共用 mJustGotShotCounter；只要确实吸收了伤害就闪本体层。
	if (remainingDamage < damage) SetGlowingTimer(kHitGlowDuration);
	if (remainingDamage <= 0 || mIsDead || !IsActive()) return;

	// 1. 优先扣除二类护盾
	if (mShieldType != ShieldType::SHIELDTYPE_NONE && !bypassShield)
	{
		const ShieldType shieldTypeBeforeHit = mShieldType;
		const int shieldHealthBeforeHit = mShieldHealth;
		int overflow = TakeShieldDamage(remainingDamage);
		if (mShieldHealth < shieldHealthBeforeHit) {
			StartShieldHitGlow(shieldTypeBeforeHit);
		}
		// 穿透（大喷菇）：护盾照常受损/掉落（触发报纸狂暴等），但全额伤害继续透到头盔+本体；
		// 阻断型护盾会吸收整次喷雾（包括破盾溢出）；普通非穿透伤害仍把击穿溢出传给后续部位。
		remainingDamage = penetrateShield ? damage : (discardShieldOverflow ? 0 : overflow);
	}

	// 2. 然后扣除头盔（穿透不绕过一类头盔，原版仅穿透二类护盾）
	if (remainingDamage > 0 && mHelmType != HelmType::HELMTYPE_NONE)
	{
		const int helmHealthBeforeHit = mHelmHealth;
		remainingDamage = TakeHelmDamage(remainingDamage);
		if (mHelmHealth < helmHealthBeforeHit) {
			SetGlowingTimer(kHitGlowDuration);
		}
	}

	// 3. 最后扣除本体
	if (remainingDamage > 0)
	{
		const int bodyHealthBeforeHit = mBodyHealth;
		TakeBodyDamage(remainingDamage);
		if (mBodyHealth < bodyHealthBeforeHit) {
			SetGlowingTimer(kHitGlowDuration);
		}
	}
}

void Zombie::HeadDrop()
{
	if (!mHasHead) return;
	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	g_particleSystem->EmitEffect("ZombieHeadOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Zombie::ArmDrop()
{
	if (!mHasArm) return;
	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	g_particleSystem->EmitEffect("ZombieArmOff",
		GetPosition());
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

void Zombie::ShieldDrop()
{
	if (mShieldType == ShieldType::SHIELDTYPE_NONE) return;
	mShieldType = ShieldType::SHIELDTYPE_NONE;
}

void Zombie::HelmDrop()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	mHelmType = HelmType::HELMTYPE_NONE;
}

void Zombie::Die()
{
	// 防重入：同帧内可能被调用两次（如自身死亡动画第 216 帧事件 + 大嘴花咬杀帧同帧命中，
	// 此刻 weak_ptr 尚未过期）。重复执行会把 mZombieNumber 多扣一次，导致计数提前归零。
	if (mIsDead) return;
	mIsDead = true;
	mToxinLayerTimers.fill(0.0f);
	mToxinDamageRemainder = 0.0f;

	// 若死亡时仍在啃食植物，手动清理啃食状态（防止 mEaterCount 无法归零）
	if (mIsEating && mEatPlantID != NULL_PLANT_ID && mBoard) {
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			plant->mEaterCount--;
		}
		mIsEating = false;
		mEatPlantID = NULL_PLANT_ID;
	}
	mEatZombieID = NULL_ZOMBIE_ID;

	if (mBoard) {
		mBoard->CollectMistFuelFromZombie(this);
		mBoard->mZombieNumber--;
		CheckWin();
	}
	// 禁用碰撞体
	if (mCollider) {
		mCollider->mEnabled = false;
	}
	this->mActive = false;
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

Vector Zombie::GetVisualPosition() const {
	return GetTransformComponent()->GetPosition()
		+ mVisualOffset + Vector(0.0f, mTangleKelpSinkOffset);
}

bool Zombie::CanBeTargetedByMagnetShroom() const
{
	return !mIsPreview && !mIsDead && !mIsDying && !mIsMindControlled
		&& mHasHead && IsActive() && HasMagneticItem();
}

Vector Zombie::GetTrackWorldPosition(const std::string& trackName) const
{
	const float scale = GetTransformComponent()
		? GetTransformComponent()->GetScale() : 1.0f;
	const Vector local = mAnimator
		? mAnimator->GetTrackPosition(trackName) : Vector::zero();
	return GetVisualPosition() + local * scale;
}

bool Zombie::CanBeTargetedByTangleKelp() const
{
	return !mIsPreview && !mIsDead && !mIsDying && !mIsMindControlled
		&& mHasHead && mInPool && mTangleKelpPlantID == NULL_PLANT_ID
		&& mCollider && mCollider->mEnabled && CanBeGrabbedByTangleKelp();
}

bool Zombie::StartTangleKelpGrab(int plantID)
{
	if (plantID == NULL_PLANT_ID) return false;
	if (mTangleKelpPlantID == plantID) {
		if (!mTangleKelpGrabBack || !mTangleKelpGrabFront) {
			CreateTangleKelpGrabAnimators();
		}
		if (ResistsTangleKelpDrowning()) {
			StopEatingForTangleKelp();
		}
		return true;
	}
	if (!CanBeTargetedByTangleKelp()) return false;

	mTangleKelpPlantID = plantID;
	mDraggedUnderByTangleKelp = false;
	mTangleKelpSinkOffset = 0.0f;
	CreateTangleKelpGrabAnimators();
	if (ResistsTangleKelpDrowning()) {
		StopEatingForTangleKelp();
	}
	return true;
}

void Zombie::DragUnderByTangleKelp(int plantID)
{
	if (mTangleKelpPlantID != plantID || mDraggedUnderByTangleKelp) return;
	mDraggedUnderByTangleKelp = true;
	StopEatingForTangleKelp();
}

void Zombie::ReleaseTangleKelpGrab(int plantID)
{
	if (mTangleKelpPlantID != plantID) return;
	ClearOrphanedTangleKelpGrab();
}

void Zombie::StopEatingForTangleKelp()
{
	if (mIsEating) {
		if (mEatPlantID != NULL_PLANT_ID && mBoard) {
			if (Plant* plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
			plant && plant->mEaterCount > 0) {
				--plant->mEaterCount;
			}
		}
		mIsEating = false;
		mEatPlantID = NULL_PLANT_ID;
		mEatZombieID = NULL_ZOMBIE_ID;
		ResumeWalkAfterEat(0.2f);
	}
}

void Zombie::CreateTangleKelpGrabAnimators(float savedFrame)
{
	auto reanim = ResourceManager::GetInstance().GetReanimation(
		ResourceKeys::Reanimations::REANIM_TANGLEKELP);
	if (!reanim) return;

	mTangleKelpGrabBack = std::make_shared<Animator>(reanim);
	mTangleKelpGrabFront = std::make_shared<Animator>(reanim);
	for (const auto& animator : { mTangleKelpGrabBack, mTangleKelpGrabFront }) {
		animator->PlayTrackOnce("anim_grab", "", kTangleKelpGrabSpeed);
		animator->SetCurrentFrame(std::clamp(
			savedFrame, kTangleKelpGrabStartFrame, kTangleKelpGrabEndFrame));
	}
	mTangleKelpGrabBack->SetTrackVisible("Layer 32", false);
	mTangleKelpGrabFront->SetTrackVisible("Layer 29", false);
}

void Zombie::ClearOrphanedTangleKelpGrab()
{
	mTangleKelpPlantID = NULL_PLANT_ID;
	mDraggedUnderByTangleKelp = false;
	mTangleKelpSinkOffset = 0.0f;
	mTangleKelpGrabBack.reset();
	mTangleKelpGrabFront.reset();
}

float Zombie::GetTangleKelpGrabFrame() const
{
	return mTangleKelpGrabFront
		? mTangleKelpGrabFront->GetCurrentFrame()
		: kTangleKelpGrabStartFrame;
}

void Zombie::EatTarget()
{
	if (mIsDying || mIsDead) return;

	if (mEatZombieID != NULL_ZOMBIE_ID && mHasHead)
	{
		Zombie* target = mBoard ? mBoard->mEntityManager.GetZombie(mEatZombieID) : nullptr;
		if (!target || target->mIsDying) {
			// 目标没了/垂死：正常由 onTriggerExit 收尾，这里兜底（含读档后目标失效）
			mIsEating = false;
			mEatZombieID = NULL_ZOMBIE_ID;
			ResumeWalkAfterEat(0.2f);
			return;
		}
		// 互啃走 TakeDamage 正常链（护盾→头盔→本体）：免伤/减伤词条对啃咬同样生效（语义自洽）；
		// 不过 ScaleZombieDamage——那是僵尸对植物的词条
		target->TakeDamage(mAttackDamage, DamageSource::ZOMBIE);
		if (GameRandom::Range(0, 1) == 0)
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.17f);
		else
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.17f);
		return;
	}
	if (mEatPlantID != NULL_PLANT_ID && mHasHead)
	{
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			// C# 原版在每次啃食伤害前重新 FindPlantTarget；这里在伤害帧兜底检查同格顶层，
			// 避免上层植物刚种下、碰撞 stay 尚未处理时仍有一口伤害落到荷叶。
			if (Plant* topPlant = mBoard->GetTopPlantAt(plant->mRow, plant->mColumn);
				topPlant && topPlant != plant && RetargetPlantIfHigherPriority(topPlant)) {
				plant = topPlant;
			}
			if (!IsCurrentPlantEatingTargetValid()) {
				StopEatingInvalidPlantTarget(0.2f);
				return;
			}
			// 魅惑菇：醒着被咬一口即触发——蘑菇立即消失、啃它的僵尸被魅惑（原版 AnimateChewSound：
			// 不结算这口伤害）。睡着（白天）不触发，走下面普通被啃路径。StartMindControlled 内部
			// 有 CanBeCharmed 守卫，对不可魅惑者自动 no-op（蘑菇照样被吃掉，与原版一致）。
			if (plant->mPlantType == PlantType::PLANT_HYPNOSHROOM && !plant->GetSleepState())
			{
				if (auto hypnoShroom = dynamic_cast<HypnoShroom*>(plant))
				{
					if (hypnoShroom->mIsEaten) return;
					hypnoShroom->mIsEaten = true;
					hypnoShroom->Die();
					AudioSystem::PlaySound("SOUND_FLOOP", 0.25f);
					this->StartMindControlled();
					return;
				}
			}
			// 原始攻击力交给植物受伤入口按来源统一结算；不写回 mAttackDamage，避免污染存档。
			plant->OnZombieBite(GetPosition());
			plant->TakeDamage(mAttackDamage, DamageSource::ZOMBIE);
			if (plant->mPlantHealth <= 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_FINISHEAT, 0.2f);
			}

			if (UsesSoftChewSound(plant->mPlantType))
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT_SOFT, 0.17f);
			}
			else if (GameRandom::Range(0, 1) == 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.17f);
			}
			else
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.17f);
			}
			if (plant->mPlantHealth <= 0) {
				StopEatingInvalidPlantTarget(0.2f);
			}
		}
		else {
			StopEatingInvalidPlantTarget(0.2f);
		}
	}
}

void Zombie::StartEat(ColliderComponent* other)
{
	// 冻结中不进入啃食态（碰撞 onTriggerStay 每帧重试，解冻后自然补上）
	if (mIsPreview || mIsDying || mFrozenTimer > 0.0f
		|| mTangleKelpPlantID != NULL_PLANT_ID) return;
	const bool wasEating = mIsEating;   // 仅"本次真开吃"（false→true）触发 OnStartEating，避免每帧 onTriggerStay 重复触发
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
	{
		auto* target = dynamic_cast<Zombie*>(gameObject);
		if (!target) return;
		if (!target->CanBeTargetedByProjectile(false)) return;
		if (target->IsMindControlled() == mIsMindControlled) return;   // 同阵营不啃（魅惑×魅惑掩码本就不成对，此为语义兜底）
		if (target->mIsDying) return;
		if (target->mRow != this->mRow) return;
		if (mEatPlantID != NULL_PLANT_ID || mEatZombieID != NULL_ZOMBIE_ID) return;   // 一次只啃一个目标

		if (!mIsEating) {
			this->PlayTrack("anim_eat", 2.1f, 0.2f);
		}
		mIsEating = true;
		mEatZombieID = target->mZombieID;
		if (!wasEating && mIsEating) OnStartEating();
		return;
	}
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (!IsPlantValidEatTarget(plant)) return;
			if (mEatZombieID != NULL_ZOMBIE_ID || plant->mRow != this->mRow) return;
			if (mEatPlantID != NULL_PLANT_ID) {
				RetargetPlantIfHigherPriority(plant);
				return;
			}

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.2f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
			if (!wasEating && mIsEating) OnStartEating();
		}
	}
}

bool Zombie::IsPlantValidEatTarget(Plant* plant) const
{
	if (!plant || !plant->CanBeEaten() || plant->mRow != mRow) return false;
	return !mBoard || mBoard->GetTopPlantAt(plant->mRow, plant->mColumn) == plant;
}

bool Zombie::RetargetPlantIfHigherPriority(Plant* plant)
{
	if (!mBoard || !mIsEating || mEatPlantID == NULL_PLANT_ID
		|| !IsPlantValidEatTarget(plant)) {
		return false;
	}

	Plant* current = mBoard->mEntityManager.GetPlant(mEatPlantID);
	if (!current || current == plant
		|| current->mRow != plant->mRow || current->mColumn != plant->mColumn) {
		return false;
	}

	// 目标切换不重播啃食动画，只迁移引用计数；旧荷叶的 exit 回调因 ID 不匹配也不会误停吃。
	if (current->mEaterCount > 0) --current->mEaterCount;
	++plant->mEaterCount;
	mEatPlantID = plant->mPlantID;
	return true;
}

bool Zombie::IsCurrentPlantEatingTargetValid() const
{
	if (!mBoard || !mIsEating || mEatPlantID == NULL_PLANT_ID
		|| !mCollider || !mCollider->mEnabled) {
		return false;
	}

	Plant* plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
	if (!plant || !plant->IsActive() || plant->mPlantHealth <= 0
		|| !IsPlantValidEatTarget(plant)) {
		return false;
	}
	const ColliderComponent* plantCollider = plant->GetColliderComponent();
	if (!plantCollider || !plantCollider->mEnabled) return false;

	const SDL_FRect zombieBounds = mCollider->GetBoundingBox();
	const SDL_FRect plantBounds = plantCollider->GetBoundingBox();
	const bool rowsOverlap = zombieBounds.y < plantBounds.y + plantBounds.h
		&& zombieBounds.y + zombieBounds.h > plantBounds.y;
	if (!rowsOverlap) return false;

	float horizontalGap = 0.0f;
	if (zombieBounds.x > plantBounds.x + plantBounds.w) {
		horizontalGap = zombieBounds.x - (plantBounds.x + plantBounds.w);
	}
	else if (plantBounds.x > zombieBounds.x + zombieBounds.w) {
		horizontalGap = plantBounds.x - (zombieBounds.x + zombieBounds.w);
	}
	return horizontalGap <= kEatingTargetRetentionGap;
}

void Zombie::StopEatingInvalidPlantTarget(float blendTime)
{
	if (!mIsEating || mEatPlantID == NULL_PLANT_ID) return;
	if (mBoard) {
		if (Plant* plant = mBoard->mEntityManager.GetPlant(mEatPlantID);
			plant && plant->mEaterCount > 0) {
			--plant->mEaterCount;
		}
	}
	mIsEating = false;
	mEatPlantID = NULL_PLANT_ID;
	ResumeWalkAfterEat(blendTime);
}

void Zombie::StopEat(ColliderComponent* other)
{
	if (mIsPreview || mIsDying)	return;
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
	{
		auto* target = dynamic_cast<Zombie*>(gameObject);
		if (!target || target->mZombieID != mEatZombieID) return;

		if (mIsEating) {
			this->ResumeWalkAfterEat(0.2f);   // 回落走路（子类可覆写选轨道+clip）
		}
		mIsEating = false;
		mEatZombieID = NULL_ZOMBIE_ID;
		return;
	}
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != plant->mPlantID || plant->mRow != this->mRow) return;

			if (mIsEating) {
				this->ResumeWalkAfterEat(0.2f);   // 收尾+回走路（经模板方法，子类钩子自动生效）
				plant->mEaterCount--;
			}
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
		}
	}
}

Vector Zombie::GetPosition() const
{
	return GetTransformComponent()->GetPosition();
}

void Zombie::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}

float Zombie::GetCurrentHorizontalMoveSpeed() const
{
	if (mIsDying || mIsDead || !mHasHead || mTangleKelpPlantID != NULL_PLANT_ID
		|| mFrozenTimer > 0.0f || !mAnimator) {
		return 0.0f;
	}
	const float trackSpeed = mGroundTrackIndex >= 0
		? mAnimator->GetTrackVelocity(mGroundTrackIndex)
		: mAnimator->GetTrackVelocity("_ground");
	float velocity = std::fabs(trackSpeed * mSpeed);
	if (mCooldownTimer > 0.0f) velocity *= 0.5f;
	if (mBoard) velocity *= mBoard->GetZombieWindMoveMultiplier(IsMovingRight());
	return std::max(0.0f, velocity);
}

float Zombie::GetTargetLeadX(float seconds) const
{
	float centerX = GetPosition().x;
	if (mCollider) {
		const SDL_FRect bounds = mCollider->GetBoundingBox();
		centerX = bounds.x + bounds.w * 0.5f;
	}
	if (seconds <= 0.0f || mIsEating || mIsDying || mIsDead || !mHasHead
		|| mTangleKelpPlantID != NULL_PLANT_ID
		|| mFrozenTimer > 0.0f || !mAnimator) {
		return centerX;
	}

	const float velocity = GetCurrentHorizontalMoveSpeed();
	return centerX + (IsMovingRight() ? velocity : -velocity) * seconds;
}

void Zombie::Draw(Graphics* g)
{
	float clipBottom = 0.0f;
	const bool clipAtWaterline = g && TryGetDrawClipBottom(clipBottom);
	if (clipAtWaterline) {
		// 水线复用通用 shader ClipRect，不生成 scissor 状态命令，因此不会把逐僵尸绘制拆成独立 draw。
		g->PushClipBottom(clipBottom);
	}

	const Vector grabPosition = GetVisualPosition()
		+ Vector(kTangleKelpGrabOffsetX, kTangleKelpGrabOffsetY);
	const float scale = GetTransformComponent()
		? GetTransformComponent()->GetScale()
		: 1.0f;
	if (mTangleKelpGrabBack) {
		mTangleKelpGrabBack->Draw(g, grabPosition.x, grabPosition.y, scale);
	}
	AnimatedObject::Draw(g);	// 水草后层之后画僵尸本体
	if (mTangleKelpGrabFront) {
		mTangleKelpGrabFront->Draw(g, grabPosition.x, grabPosition.y, scale);
	}

	// 冻结冰晶（icetrap.png）：画在本体之后=前景，垫在僵尸脚底
	// （原版分前后两张 ICETRAP/ICETRAP2，本项目单图取前层简化）
	if (g && mFrozenTimer > 0.0f && !mIsPreview)
	{
		if (auto* tex = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_ICETRAP))
		{
			const Vector pos = GetPosition();
			const float w = static_cast<float>(tex->width);
			const float h = static_cast<float>(tex->height);
			// 僵尸判定矩形底边 ≈ y+35：冰晶底边压在脚底线上（站位截图后微调）
			g->DrawTexture(tex, pos.x - w * 0.5f, pos.y + 35.0f - h, w, h);
		}
	}

	if (clipAtWaterline) {
		g->PopClipBottom();
	}

	if (!g || mIsPreview || !GameAPP::GetInstance().mShowZombieHP) return;
	// 视口剔除：屏外僵尸不画血量文字。11000 压测下绝大多数僵尸堆在屏幕外，
	// 这些 DrawText 会把 ~40 万文字 quad 砸进 128MB batch VBO 致溢出——剔除后省 VBO + CPU。
	if (!g->IsWorldPointVisible(GetPosition().x, GetPosition().y)) return;

	// 直接用逻辑坐标：DrawText 与 Animator 的 DrawTextureMatrix 共享同一 projView，
	// Animator 画 sprite 时就是用裸逻辑坐标，文字必须同坐标系才能叠在对象上（勿转 World）
	Vector pos = GetPosition() + Vector(-40, -40);

	constexpr int   fontSize = 15;
	constexpr float lineHeight = 18.0f;	// 行距 ≈ 字号，逐行向下、无空行
	// 颜色是 0..255 范围（ToSDLColor 直接 static_cast，不乘 255），勿写成 0..1 否则全透明隐形
	const glm::vec4 lightBlue(150.0f, 200.0f, 255.0f, 255.0f);

	float y = pos.y;
	auto drawLine = [&](const std::string& text) {
		g->DrawGlyphRun(text, ResourceKeys::Fonts::FONT_FZJZ, fontSize, lightBlue, pos.x, y);
		y += lineHeight;
		};

	// 本体（始终显示）
	drawLine(u8"本体: " + std::to_string(mBodyHealth) + u8"/" + std::to_string(mBodyMaxHealth));
	// 一类防具（有 mHelmType 才显示）
	if (mHelmType != HelmType::HELMTYPE_NONE)
		drawLine(u8"一类: " + std::to_string(mHelmHealth) + u8"/" + std::to_string(mHelmMaxHealth));
	// 二类防具（有 mShieldType 才显示）
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
		drawLine(u8"二类: " + std::to_string(mShieldHealth) + u8"/" + std::to_string(mShieldMaxHealth));
}

void Zombie::ValidateEatingState(EntityManager& em)
{
	// 旧档可能把“正在啃食”与加固铁门的水草束缚同时保存；关系恢复后必须立即结束啃食。
	if (mTangleKelpPlantID != NULL_PLANT_ID && ResistsTangleKelpDrowning()) {
		StopEatingForTangleKelp();
		return;
	}

	if (mIsEating && mEatPlantID != NULL_PLANT_ID) {
		auto plant = em.GetPlant(mEatPlantID);
		if (!plant) {
			mIsEating = false;
			mEatPlantID = NULL_PLANT_ID;
			ResumeWalkAfterEat(0.3f);   // 收尾+回走路（经模板方法，子类钩子自动生效）
		}
		else {
			plant->mEaterCount++;
		}
	}
	else if (mIsEating) {
		// mEatPlantID 为空却在啃：啃僵尸进行时存的档（mEatZombieID 不持久化）→ 回走路，碰撞下一帧重建互啃
		mIsEating = false;
		ResumeWalkAfterEat(0.3f);
	}
}

bool Zombie::TryGetDrawClipBottom(float& clipBottom) const
{
	if (!mInPool || mIsPreview) return false;
	clipBottom = static_cast<float>(static_cast<int>(
		std::lround(GetPosition().y + kPoolClipBottomOffsetY)));
	return true;
}
