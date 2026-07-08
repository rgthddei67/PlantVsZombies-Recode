#include "ScaredyShroom.h"
#include "../ShadowComponent.h"

void ScaredyShroom::SetupPlant()
{
	auto shadow = GetComponent<ShadowComponent>();
	shadow->SetScale(Vector(0.6f, 0.6f));
	shadow->SetOffset(Vector(3, 30));

	Shroom::SetupPlant();

	if (mIsPreview) return;

	// 第 25 帧吐孢子（主人按动画预览指定；anim_shooting 活跃区间为全局 16..30 帧）
	mAnimator->AddFrameEvent(25, [this]() {
		if (!mBoard) return;
		AudioSystem::PlaySound("SOUND_PUFF", 0.28f);
		Vector bulletPosition = GetPosition() + Vector(2, -6);
		mBoard->CreateBullet(BulletType::BULLET_PUFF, mRow, bulletPosition);
		}, true);
}

void ScaredyShroom::PlantUpdate()
{
	// 射击动画进行中不进害怕流程（原版 mShootingCounter>0 时 return 的等价守卫），
	// 否则缩头会把吐孢子动画拦腰打断。
	const std::string curTrack = GetCurrentTrackName();
	if (curTrack != "anim_shooting") {
		const bool scared = HasZombieNearby();
		switch (mFearState) {
		case FearState::READY:
			if (scared) {
				mFearState = FearState::LOWERING;
				PlayTrackOnce("anim_scared", "anim_scaredidle", 0.0f, 0.2f);
			}
			break;
		case FearState::LOWERING:
			// PlayTrackOnce 播完自动接 anim_scaredidle，以轨道名切换为完成信号
			if (curTrack == "anim_scaredidle") mFearState = FearState::SCARED;
			break;
		case FearState::SCARED:
			if (!scared) {
				mFearState = FearState::RAISING;
				PlayTrackOnce("anim_grow", "anim_idle", 0.0f, 0.2f);
			}
			break;
		case FearState::RAISING:
			if (curTrack == "anim_idle") mFearState = FearState::READY;
			break;
		}
	}

	if (mFearState != FearState::READY) {
		mShootTimer = 0.0f;   // 非就绪态压住射击计时（原版 mLaunchCounter=mLaunchRate）
		return;
	}

	// 词条：植物攻速。mult>=1（非生存关/未获取恒为 1.0）。
	float mult = mBoard ? static_cast<float>(mBoard->GetPerkManager().GetPlantAttackSpeedMultiplier()) : 1.0f;
	this->mShootTimer += (DeltaTime::GetDeltaTime() * mult);
	if (this->mShootTimer >= this->mShootTime)
	{
		if (HasZombieInRow())
		{
			mShootTimer = 0;
			// 动画同比例加快：吐弹的第 25 帧 frame event 跟上更短间隔
			PlayTrackOnce("anim_shooting", "anim_idle", 1.5f * mult, 0.2f);
		}
	}
}

bool ScaredyShroom::HasZombieInRow()
{
	if (mBoard)
	{
		mCheckZombieTimer += DeltaTime::GetDeltaTime();
		if (mCheckZombieTimer >= 0.6f)
		{
			mCheckZombieTimer = 0.0f;
			// 按行索引：只遍历本行僵尸。全行射程，不设 300px 上限（与小喷菇的差异点）
			const float thisX = GetPosition().x;
			bool found = false;
			mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
				if (found) return;  // 已命中，跳过本行其余
				float dx = zombie->GetPosition().x - thisX;
				if (!zombie->IsMindControlled() && dx >= 0 && zombie->HasHead())
					found = true;
			});
			return found;
		}
	}
	return false;
}

bool ScaredyShroom::HasZombieNearby()
{
	if (!mBoard) return false;

	// 0.1s 节流：害怕判定要扫 3 行僵尸做圆-矩形相交，逐帧算在海量僵尸场景下白烧 CPU，
	// 100ms 的反应延迟肉眼不可感。缓存上次结果供节流间隙使用。
	mFearCheckTimer += DeltaTime::GetDeltaTime();
	if (mFearCheckTimer < 0.1f) return mScaredCached;
	mFearCheckTimer = 0.0f;

	// 原版圆心 (mX, mY+20) 是格子左上角坐标系；本项目 GetPosition() 是 80x100 格子中心，
	// 换算后圆心 = 中心 + (-40, -30)，半径 120（覆盖本行±1行、含身后僵尸）。
	const Vector center = GetPosition() + Vector(-40.0f, -30.0f);
	constexpr float kFearRadiusSq = 120.0f * 120.0f;

	bool found = false;
	for (int row = mRow - 1; row <= mRow + 1 && !found; ++row) {
		// 行索引已排除失活/濒死僵尸（原版也不吓垂死僵尸）
		mBoard->mEntityManager.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (found) return;
			if (zombie->IsMindControlled()) return;   // 魅惑僵尸不吓
			auto collider = zombie->GetColliderComponent();
			if (!collider) return;
			const SDL_FRect rect = collider->GetBoundingBox();
			// 圆 vs 矩形：取矩形上离圆心最近的点判距
			const float nx = std::clamp(center.x, rect.x, rect.x + rect.w);
			const float ny = std::clamp(center.y, rect.y, rect.y + rect.h);
			const float dx = center.x - nx;
			const float dy = center.y - ny;
			if (dx * dx + dy * dy <= kFearRadiusSq)
				found = true;
		});
	}
	mScaredCached = found;
	return found;
}
