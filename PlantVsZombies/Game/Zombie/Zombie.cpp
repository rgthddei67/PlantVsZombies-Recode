#include "Zombie.h"
#include "ZombieCharred.h"
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
	j["extraSpeed"] = mExtraSpeed;   // extra 速度层基准（如狂暴报纸僵尸 2.5、铁桶快僵随机值）
	j["cooldownTimer"] = mCooldownTimer;
	j["frozenTimer"] = mFrozenTimer;
	j["dyingTimer"] = mDyingTimer;
}

void Zombie::LoadProtectedData(const nlohmann::json& j) {
	mIsMindControlled = j.value("isMindControlled", false);
	// 魅惑是持久状态，掩码/红光是派生状态：读档按标志重建（存档只存布尔）
	if (mIsMindControlled) {
		ApplyCharmEffects();
	}
	mFreeHitsRemaining = j.value("freeHitsRemaining", 0);   // 旧档缺字段→0
	mIsEating = j.value("isEating", false);
	mEatPlantID = j.value("eatPlantID", NULL_PLANT_ID);
	mHasHead = j.value("hasHead", true);
	mHasArm = j.value("hasArm", true);
	mHasTongue = j.value("hasTongue", false);
	mIsDying = j.value("isDying", false);
	mInPool = j.value("inPool", false);
	mSpeed = j.value("speed", 10.0f);

	// 必须在 SetCooldown 之前恢复：SetCooldown 内部用 mExtraSpeed*0.6 设置减速倍率，
	// 若此刻 mExtraSpeed 仍是默认 1.0，减速会被错算（应为 base*0.6）。
	mExtraSpeed = j.value("extraSpeed", 1.0f);
	float cooldown = j.value("cooldownTimer", 0.0f);
	if (cooldown > 0.0f) {
		this->SetCooldown(cooldown);                       // 用已恢复的 mExtraSpeed，正确得到 base*0.6
	}
	else if (mAnimator) {
		UpdateAnimSpeed();   // 无减速也要恢复僵尸基准 × 当前雨势倍率
	}

	// 冻结还原：必须在减速恢复之后——UpdateAnimSpeed 里冻结优先，把 extra 覆盖回 0（停格）。
	mFrozenTimer = j.value("frozenTimer", 0.0f);
	if (mFrozenTimer > 0.0f && mAnimator) {
		UpdateAnimSpeed();
		// 冻结自带蓝光：持盾僵尸吃不到上面 SetCooldown 那份（它直接 no-op），这里补上
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}

	// 如果播放死亡动画，禁用碰撞箱（判空与 Die/预览路径一致：预览僵尸已移除碰撞箱、mCollider=null）
	if (mIsDying && mCollider) {
		mCollider->mEnabled = false;
	}

	mDyingTimer = j.value("dyingTimer", 0.0f);
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
	// SetupZombie 可设置品种基准 mExtraSpeed；最后统一叠加减速/冻结/雨势，且跨 PlayTrack 存活。
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
	if (!mIsPreview) {
		float deltaTime = DeltaTime::GetDeltaTime();
		auto* transform = this->GetTransformComponent();

		if (!transform || !mBoard) return;

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
				if (mAnimator) mAnimator->EnableOverlayEffect(false);
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
			if (position.x <= GAMEOVER_X && mBoard->mBoardState == BoardState::GAME)
			{
				mBoard->GameOver();
			}
			if (position.x > static_cast<float>(SCENE_WIDTH + 65) || position.x < -20.0f)
			{
				this->Die();
			}
		}

		// 阵风是空气施加的独立位移：在冻结/啃食的早退前结算，使碰撞箱随 Transform 同帧移动。
		// 若被吹离啃食目标，碰撞退出回调会按正常链路停止啃食，不需另造状态分支。
		ApplyTyphoonGustDrift(deltaTime, transform);
		// 入水状态是通用介质状态：冻结或啃食期间也要跟随阵风后的实际位置更新。
		if (!mIsDying) UpdatePoolState();

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
					// 冻着也要能倒：先解停格再播死亡动画（帧事件 Die 依赖动画前进）
					if (mFrozenTimer > 0.0f) ClearFrozen();
					PlayTrack(GetDeathTrackName(), 1.3f, 0.3f);
					if (mCollider) mCollider->mEnabled = false;
					mIsDying = true;
				}
				return;
			}
		}

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
		if (mBoard) speed *= mBoard->GetZombieWindMoveMultiplier(mIsMindControlled);
		if (mIsMindControlled)
		{
			transform->Translate(speed * scaledDelta, 0);
		}
		else
		{
			transform->Translate(-speed * scaledDelta, 0);
		}
	}
}

void Zombie::SetCooldown(float timer)
{
	if (!mAnimator || mShieldType != ShieldType::SHIELDTYPE_NONE) return;

	// 首次进入减速：蓝色 overlay。速度经 UpdateAnimSpeed 统一收敛（extra 层，
	// 后续 PlayTrack / SetSpeed 不会覆盖；冻结中保持停格不被顶掉）。
	if (mCooldownTimer <= 0.0f)
	{
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}

	// 已在减速中则取 max，避免短射缩短减速
	mCooldownTimer = std::max(mCooldownTimer, timer);
	UpdateAnimSpeed();
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
		mExtraSpeed * GetAbilityAnimSpeedMultiplier()
		* (mCooldownTimer > 0.0f ? GetSlowAnimFactor() : 1.0f) * rainMultiplier);
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

	// 冻结自带蓝色 overlay（与减速共用同色）：持盾僵尸吃不到 SetCooldown 的那份，也要变蓝
	if (mAnimator)
	{
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}

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
	// 减速尾巴仍在则蓝光归它管（到期自清）；没有尾巴（持盾）立即褪色
	if (mCooldownTimer <= 0.0f && mAnimator)
	{
		mAnimator->EnableOverlayEffect(false);
	}
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
	// 视觉：红光。overlay 与减速蓝光共用——调用方已保证减速被清、且魅惑后子弹打不中不会再有新减速。
	if (mAnimator) {
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(255, 64, 64, 160);
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
	int damage, DamageSource source, bool penetrateShield, bool discardShieldOverflow)
{
	if (damage <= 0 || !mBoard) return;

	// 词条②：僵尸前 N 次免伤（生存专用）。出生时由词条层数设定 mFreeHitsRemaining。
	// 提前 return：完全吸收且不触发受击白光（SetGlowingTimer），0 伤害不应闪。
	if (mFreeHitsRemaining > 0) { --mFreeHitsRemaining; return; }

	// 植物增伤只放大植物来源（普通/灰烬）；僵尸免伤则对所有实际承伤生效。两者均在 0 层返回单位元。
	if (source == DamageSource::PLANT || source == DamageSource::PLANT_ASH) {
		damage = mBoard->GetPerkManager().ScalePlantDamage(damage);
	}
	damage = mBoard->GetPerkManager().ScaleDamageToZombie(damage);
	damage = AdjustIncomingDamage(damage, source, penetrateShield);
	if (damage <= 0) return;

	SetGlowingTimer(0.1f);

	int remainingDamage = damage;

	// 1. 优先扣除二类护盾
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		int overflow = TakeShieldDamage(remainingDamage);
		// 穿透（大喷菇）：护盾照常受损/掉落（触发报纸狂暴等），但全额伤害继续透到头盔+本体；
		// 阻断型护盾会吸收整次喷雾（包括破盾溢出）；普通非穿透伤害仍把击穿溢出传给后续部位。
		remainingDamage = penetrateShield ? damage : (discardShieldOverflow ? 0 : overflow);
	}

	// 2. 然后扣除头盔（穿透不绕过一类头盔，原版仅穿透二类护盾）
	if (remainingDamage > 0 && mHelmType != HelmType::HELMTYPE_NONE)
	{
		remainingDamage = TakeHelmDamage(remainingDamage);
	}

	// 3. 最后扣除本体
	if (remainingDamage > 0)
	{
		TakeBodyDamage(remainingDamage);
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
	return GetTransformComponent()->GetPosition() + mVisualOffset;
}

void Zombie::EatTarget()
{
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
			plant->TakeDamage(mAttackDamage, DamageSource::ZOMBIE);
			if (plant->mPlantHealth <= 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_FINISHEAT, 0.2f);
			}

			int random = GameRandom::Range(0, 1);
			if (random == 0)
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.17f);
			}
			else
			{
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.17f);
			}
		}
	}
}

void Zombie::StartEat(ColliderComponent* other)
{
	// 冻结中不进入啃食态（碰撞 onTriggerStay 每帧重试，解冻后自然补上）
	if (mIsPreview || mIsDying || mFrozenTimer > 0.0f)	return;
	const bool wasEating = mIsEating;   // 仅"本次真开吃"（false→true）触发 OnStartEating，避免每帧 onTriggerStay 重复触发
	auto* gameObject = other->GetGameObject();
	if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
	{
		auto* target = dynamic_cast<Zombie*>(gameObject);
		if (!target) return;
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

void Zombie::Draw(Graphics* g)
{
	const bool clipAtWaterline = g && mInPool && !mIsPreview;
	if (clipAtWaterline) {
		// Draw 内嵌套裁剪，不占用 GameObject 的单一 ClipRect；可与伴舞出土等对象裁剪安全求交。
		const int clipBottom = static_cast<int>(
			std::lround(GetPosition().y + kPoolClipBottomOffsetY));
		g->PushClipRect(0, 0, SCENE_WIDTH, clipBottom);
	}

	AnimatedObject::Draw(g);	// 先画本体动画

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
		g->PopClipRect();
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
