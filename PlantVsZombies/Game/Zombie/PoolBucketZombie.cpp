#include "PoolBucketZombie.h"

/** 初始化铁桶耐久，同时保留泳池僵尸的事件与稳态轨道设置。 */
void PoolBucketZombie::SetupZombie()
{
	PoolNormalZombie::SetupZombie();
	mHelmHealth = 1100;
	mHelmMaxHealth = 1100;
	mHelmType = HelmType::HELMTYPE_BUCKET;
}

/** 铁桶完全掉落时隐藏轨道，并沿用现有掉落特效。 */
void PoolBucketZombie::HelmDrop()
{
	Zombie::HelmDrop();
	mHelmStage = ArmorBrokenState::NONE;
	mAnimator->SetTrackVisible("anim_bucket", false);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("ZombieBucketOff", GetPosition());
	}
}

/** 按剩余耐久切换铁桶的两级破损贴图。 */
void PoolBucketZombie::CheckHelmImage()
{
	if (mHelmType == HelmType::HELMTYPE_NONE) return;
	if (mHelmStage == ArmorBrokenState::NO_BROKEN &&
		mHelmHealth <= static_cast<int64_t>(mHelmMaxHealth) * 2 / 3) {
		mHelmStage = ArmorBrokenState::A_LITTLE_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET2"));
	}
	if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN && mHelmHealth <= mHelmMaxHealth / 3) {
		mHelmStage = ArmorBrokenState::REALLY_BROKEN;
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET3"));
	}
}

/** 组合保存泳池状态与头盔破损阶段。 */
void PoolBucketZombie::SaveExtraData(nlohmann::json& j) const
{
	PoolNormalZombie::SaveExtraData(j);
	j["helmStage"] = static_cast<int>(mHelmStage);
}

/** 组合恢复泳池状态与头盔破损阶段，并兼容缺字段旧档。 */
void PoolBucketZombie::LoadExtraData(const nlohmann::json& j)
{
	PoolNormalZombie::LoadExtraData(j);
	mHelmStage = static_cast<ArmorBrokenState>(
		j.value("helmStage", static_cast<int>(ArmorBrokenState::NO_BROKEN)));
}

/** 读档后重建铁桶显隐与破损贴图。 */
void PoolBucketZombie::ZombieItemUpdate() const
{
	PoolNormalZombie::ZombieItemUpdate();
	if (mHelmStage == ArmorBrokenState::NONE || mHelmType == HelmType::HELMTYPE_NONE) {
		mAnimator->SetTrackVisible("anim_bucket", false);
	}
	else if (mHelmStage == ArmorBrokenState::A_LITTLE_BROKEN) {
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET2"));
	}
	else if (mHelmStage == ArmorBrokenState::REALLY_BROKEN) {
		mAnimator->SetTrackImage("anim_bucket", ResourceManager::GetInstance().
			GetTexture("IMAGE_ZOMBIE_BUCKET3"));
	}
}
