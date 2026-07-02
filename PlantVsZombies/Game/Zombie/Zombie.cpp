#include "Zombie.h"
#include "ZombieCharred.h"
#include "../Plant/Plant.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../GameObjectManager.h"
#include "../Plant/GameDataManager.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../GameApp.h"
#include <climits>

namespace {
	float GAMEOVER_X = 110.0f;	// 僵尸到达此横坐标即触发游戏失败
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

	if (isPreview)
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
	mAnimator->AddFrameEvent(216, [this]() { this->Die(); });
	mAnimator->AddFrameEvent(152, [this]() { this->EatTarget(); }, true);
	mAnimator->AddFrameEvent(171, [this]() { this->EatTarget(); }, true);

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
	j["speed"] = mSpeed;
	j["extraSpeed"] = mExtraSpeed;   // extra 速度层基准（如狂暴报纸僵尸 2.5、铁桶快僵随机值）
	j["cooldownTimer"] = mCooldownTimer;
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
	mSpeed = j.value("speed", 10.0f);

	// 必须在 SetCooldown 之前恢复：SetCooldown 内部用 mExtraSpeed*0.6 设置减速倍率，
	// 若此刻 mExtraSpeed 仍是默认 1.0，减速会被错算（应为 base*0.6）。
	mExtraSpeed = j.value("extraSpeed", 1.0f);
	float cooldown = j.value("cooldownTimer", 0.0f);
	if (cooldown > 0.0f) {
		this->SetCooldown(cooldown);                       // 用已恢复的 mExtraSpeed，正确得到 base*0.6
	}
	else if (mAnimator) {
		mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);   // 无减速：直接应用基准（狂暴 2.5 等）
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
}

void Zombie::Charred()
{
	GameObjectManager::GetInstance().CreateGameObjectImmediate<ZombieCharred>
		(LAYER_GAME_ZOMBIE, ObjectType::OBJECT_ZOMBIE, mBoard, this->GetVisualPosition(), AnimationType::ANIM_ZOMBIE_CHARRED);
	Die();
}

void Zombie::Start()
{
	AnimatedObject::Start();
	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	shadowcomponent->SetDrawOrder(-80);
	if (this->mIsPreview) {
		RemoveComponent<ColliderComponent>();
		mCollider = nullptr;  // 缓存的裸指针随之失效，显式置空
	}
	SetAnimationSpeed(GameRandom::Range(1.1f, 1.4f));
	SetupZombie();
}

void Zombie::CheckWin() const
{
	if (mBoard && mBoard->mCurrentWave >= mBoard->mMaxWave && mBoard->mZombieNumber <= 0)
	{
		if (mBoard->mIsSurvival)
			mBoard->OnSurvivalRoundClear();   // 生存模式：进入下一轮，不出奖杯
		else
			mBoard->CreateTrophy(GetPosition());
	}
}

void Zombie::Update()
{
	AnimatedObject::Update();
	if (!mIsPreview) {
		float deltaTime = DeltaTime::GetDeltaTime();
		auto* transform = this->GetTransformComponent();

		if (!transform && !mBoard) return;

		if (mIsDying)
		{
			mDyingTimer += deltaTime;
			if (GetCurrentTrackName() != "anim_death" && !mDbgAnomalyLogged) {
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
				if (mAnimator)
				{
					mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
					mAnimator->EnableOverlayEffect(false);
				}
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
					PlayTrack("anim_death", 1.3f, 0.3f);
					if (mCollider) mCollider->mEnabled = false;
					mIsDying = true;
				}
				return;
			}
		}

		if (mIsEating) return;

		ZombieMove(scaledDelta, transform);
		ZombieUpdate(scaledDelta);
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

	// 首次进入减速：启用独立的 0.6x 速度倍率 + 蓝色 overlay。
	// 用 mExtraSpeedMultiplier 而非 SetSpeed，这样后续 PlayTrack / SetSpeed
	// （例如切换啃食、死亡动画）不会覆盖减速效果。
	if (mCooldownTimer <= 0.0f)
	{
		mAnimator->SetExtraSpeedMultiplier(mExtraSpeed * 0.6f);
		mAnimator->EnableOverlayEffect(true);
		mAnimator->SetOverlayColor(80, 80, 255, 240);
	}

	// 已在减速中则取 max，避免短射缩短减速
	mCooldownTimer = std::max(mCooldownTimer, timer);
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

void Zombie::ResumeWalkAfterEat(float blendTime)
{
	// clip 清零 → EffectiveSpeed 回落到 base 走速（普通僵尸的 anim_walk2 无需 clip）。
	PlayTrack(WalkTrackAfterEat(), 0.0f, blendTime);
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

	// 清减速：把 overlay 让给红光；魅惑后子弹打不中，减速不会再来
	if (mCooldownTimer > 0.0f) {
		mCooldownTimer = 0.0f;
		if (mAnimator) mAnimator->SetExtraSpeedMultiplier(mExtraSpeed);
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

void Zombie::TakeDamage(int damage, bool penetrateShield)
{
	if (damage <= 0 || !mBoard) return;

	// 词条②：僵尸前 N 次免伤（生存专用）。出生时由词条层数设定 mFreeHitsRemaining。
	// 提前 return：完全吸收且不触发受击白光（SetGlowingTimer），0 伤害不应闪。
	if (mFreeHitsRemaining > 0) { --mFreeHitsRemaining; return; }

	// 词条：植物增伤 僵尸免伤（生存专用；空词条/非生存关倍率=1，无副作用）。单点覆盖一切伤害来源。
	damage = mBoard->GetPerkManager().ScaleTotalDamageToZombie(damage);

	SetGlowingTimer(0.1f);

	int remainingDamage = damage;

	// 1. 优先扣除二类护盾
	if (mShieldType != ShieldType::SHIELDTYPE_NONE)
	{
		int overflow = TakeShieldDamage(remainingDamage);
		// 穿透（大喷菇）：护盾照常受损/掉落（触发报纸狂暴等），但全额伤害继续透到头盔+本体；
		// 非穿透时维持原行为——只有击穿护盾后的溢出伤害才进入头盔/本体。
		remainingDamage = penetrateShield ? damage : overflow;
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
		target->TakeDamage(mAttackDamage);
		if (GameRandom::Range(0, 1) == 0)
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT, 0.17f);
		else
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIE_EAT2, 0.17f);
		return;
	}
	if (mEatPlantID != NULL_PLANT_ID && mHasHead)
	{
		if (auto* plant = mBoard->mEntityManager.GetPlant(mEatPlantID)) {
			// 魅惑菇：醒着被咬一口即触发——蘑菇立即消失、啃它的僵尸被魅惑（原版 AnimateChewSound：
			// 不结算这口伤害）。睡着（白天）不触发，走下面普通被啃路径。StartMindControlled 内部
			// 有 CanBeCharmed 守卫，对不可魅惑者自动 no-op（蘑菇照样被吃掉，与原版一致）。
			if (plant->mPlantType == PlantType::PLANT_HYPNOSHROOM && !plant->GetSleepState())
			{
				plant->Die();
				AudioSystem::PlaySound("SOUND_FLOOP", 0.25f);
				StartMindControlled();
				return;
			}
			// 词条①：僵尸对植物伤害（生存专用；空词条倍率=1）。使用时缩放，不写回 mAttackDamage——
			// 否则存档 attackDamage 被污染，读档叠加重复放大。mBoard 在此路径恒非空（上一行已解引用）。
			plant->TakeDamage(mBoard->GetPerkManager().ScaleZombieDamage(mAttackDamage));
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
	if (mIsPreview || mIsDying)	return;
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
		return;
	}
	if (gameObject->GetObjectType() == ObjectType::OBJECT_PLANT)
	{
		if (auto* plant = dynamic_cast<Plant*>(gameObject))
		{
			if (mEatPlantID != NULL_PLANT_ID || mEatZombieID != NULL_ZOMBIE_ID || plant->mRow != this->mRow) return;	// 正在吃一个目标，那么不吃别的

			if (!mIsEating) {
				this->PlayTrack("anim_eat", 2.1f, 0.2f);
			}
			mIsEating = true;
			mEatPlantID = plant->mPlantID;
			plant->mEaterCount++;
		}
	}
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
				this->PlayTrack("anim_walk2", 0.0f, 0.2f);   // clip 清零，自动回落走速
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
	AnimatedObject::Draw(g);	// 先画本体动画

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
			PlayTrack("anim_walk2", 0.0f, 0.3f);   // clip 清零，自动回落走速
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