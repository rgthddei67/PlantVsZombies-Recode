#include "PoolNormalZombie.h"

#include "../AudioSystem.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"

namespace {
	constexpr int POOL_LAND_DEATH_FRAME = 216;     // 陆地死亡动画的结束帧
	constexpr int POOL_WATER_DEATH_FRAME = 283;    // 水中死亡动画的结束帧
	constexpr int POOL_FIRST_BITE_FRAME = 147;     // 泳池 reanim 第一次啃食命中帧
	constexpr int POOL_SECOND_BITE_FRAME = 168;    // 泳池 reanim 第二次啃食命中帧
	constexpr float POOL_FRONT_PROBE_X = 75.0f;    // 身体前侧入水检测点相对世界 X 偏移
	constexpr float POOL_REAR_PROBE_X = 45.0f;     // 身体后侧入水检测点相对世界 X 偏移
	constexpr float POOL_TRANSITION_RIGHT_SHIFT_X = 70.0f; // 适配本项目泳池图，把下水/出水切换位置向右校正的像素数
	constexpr float POOL_TRANSITION_BLEND = 0.2f;  // 入水、离水切换稳态动画的混合秒数
	constexpr const char* POOL_LEG_TRACKS[] = {    // 啃食时需藏到水线下的六条腿部轨道
		"Zombie_innerleg_upper",
		"Zombie_innerleg_lower",
		"Zombie_innerleg_foot",
		"Zombie_outerleg_upper",
		"Zombie_outerleg_lower",
		"Zombie_outerleg_foot",
	};
}

/** 缓存基类创建的阴影组件，供入水状态和读档恢复统一控制。 */
void PoolNormalZombie::Start()
{
	Zombie::Start();
	mPoolShadow = GetComponent<ShadowComponent>();
	UpdatePoolVisualState();
}

/** 初始化泳池僵尸，并确保陆地稳态固定使用 walk2。 */
void PoolNormalZombie::SetupZombie()
{
	Zombie::SetupZombie();
	if (!mIsPreview) PlayWalkAnimation();
}

/** 注册泳池僵尸 reanim 的陆地/水中死亡终点和两次啃食命中帧。 */
void PoolNormalZombie::RegisterFrameEvents()
{
	mAnimator->AddFrameEvent(POOL_LAND_DEATH_FRAME, [this]() { Die(); });
	mAnimator->AddFrameEvent(POOL_WATER_DEATH_FRAME, [this]() { Die(); });
	mAnimator->AddFrameEvent(POOL_FIRST_BITE_FRAME, [this]() { EatTarget(); }, true);
	mAnimator->AddFrameEvent(POOL_SECOND_BITE_FRAME, [this]() { EatTarget(); }, true);
}

/** 非啃食状态下逐帧检测水域边界。 */
void PoolNormalZombie::ZombieUpdate(float scaledTime)
{
	(void)scaledTime;
	if (!mIsDying) UpdatePoolState();
}

/** 仅当校正后的身体前后探针都落入水域时视为入水，避免在池沿来回抖动。 */
void PoolNormalZombie::UpdatePoolState()
{
	if (!mBoard || mIsPreview || mRow < 0) return;

	const float x = GetPosition().x;
	// 探针相对僵尸左移，会让僵尸自身到达更靠右的位置时切换，匹配本项目泳池图的池沿。
	const bool shouldBeInPool =
		mBoard->IsPoolWorldPosition(mRow,
			x + POOL_FRONT_PROBE_X - POOL_TRANSITION_RIGHT_SHIFT_X) &&
		mBoard->IsPoolWorldPosition(mRow,
			x + POOL_REAR_PROBE_X - POOL_TRANSITION_RIGHT_SHIFT_X);
	if (shouldBeInPool == mInPool) return;

	mInPool = shouldBeInPool;
	UpdatePoolVisualState();
	PlayWalkAnimation(POOL_TRANSITION_BLEND);
}

/** 水中使用 swim，离水后恢复固定的 walk2。 */
void PoolNormalZombie::PlayWalkAnimation(float blendTime)
{
	PlayTrack(mInPool ? "anim_swim" : "anim_walk2", 0.0f, blendTime);
}

/** 水中啃食轨道会重现双腿并换回无涟漪泳圈；切轨后统一恢复水中表现。 */
void PoolNormalZombie::OnStartEating()
{
	SetPoolEatingVisuals(mInPool);
}

/** 停止啃食后撤销视觉覆盖，让 walk2/swim 轨道继续按自身帧数据控制。 */
void PoolNormalZombie::OnStopEating()
{
	SetPoolEatingVisuals(false);
}

/** 掉头流血致死时按当前介质选择对应死亡轨道。 */
const char* PoolNormalZombie::GetDeathTrackName() const
{
	return mInPool ? "anim_waterdeath" : "anim_death";
}

/** 水中掉头只更新人物轨道并播放音效，不生成悬浮的陆地碎片。 */
void PoolNormalZombie::HeadDrop()
{
	if (!mInPool) {
		Zombie::HeadDrop();
		return;
	}
	if (!mHasHead) return;

	mAnimator->SetTrackVisible("anim_head1", false);
	mAnimator->SetTrackVisible("anim_head2", false);
	mAnimator->SetTrackVisible("anim_tongue", false);
	mAnimator->SetTrackVisible("anim_hair", false);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

/** 水中断臂保留残臂贴图变化，但不生成陆地掉臂粒子。 */
void PoolNormalZombie::ArmDrop()
{
	if (!mInPool) {
		Zombie::ArmDrop();
		return;
	}
	if (!mHasArm) return;

	mAnimator->SetTrackVisible("Zombie_outerarm_hand", false);
	mAnimator->SetTrackVisible("Zombie_outerarm_lower", false);
	mAnimator->SetTrackImage("Zombie_outerarm_upper", ResourceManager::GetInstance().
		GetTexture(ResourceKeys::Textures::IMAGE_ZOMBIE_OUTERARM_UPPER2));
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ARM_HEAD_DROP, 0.25f);
}

/** 保存泳池介质状态，使读档后死亡轨道和稳态动画保持一致。 */
void PoolNormalZombie::SaveExtraData(nlohmann::json& j) const
{
	j["inPool"] = mInPool;
}

/** 兼容旧档：缺少 inPool 时按陆地状态恢复，下一逻辑帧再由场景校正。 */
void PoolNormalZombie::LoadExtraData(const nlohmann::json& j)
{
	mInPool = j.value("inPool", false);
}

/** 读档后重建断肢轨道，并同步水中阴影。 */
void PoolNormalZombie::ZombieItemUpdate() const
{
	Zombie::ZombieItemUpdate();
	// 基类最后会按随机舌头标志恢复舌头；无头泳池僵尸必须再次收敛为整组隐藏。
	if (!mHasHead) {
		mAnimator->SetTrackVisible("anim_head1", false);
		mAnimator->SetTrackVisible("anim_head2", false);
		mAnimator->SetTrackVisible("anim_tongue", false);
		mAnimator->SetTrackVisible("anim_hair", false);
	}
	UpdatePoolVisualState();
}

/** 水中隐藏地面投影；读档处于啃食态时同时重建双腿和涟漪泳圈。 */
void PoolNormalZombie::UpdatePoolVisualState() const
{
	if (mPoolShadow) {
		mPoolShadow->SetVisible(!mInPool);
	}
	SetPoolEatingVisuals(mInPool && mIsEating);
}

/** 只在水中啃食期间覆盖泳圈贴图，既保留涟漪又不污染其他动画片段。 */
void PoolNormalZombie::SetPoolEatingVisuals(bool active) const
{
	if (!mAnimator) return;
	SetPoolLegsVisible(!active);
	const Texture* tubeImage = active
		? ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_REANIM_ZOMBIE_DUCKYTUBE_INWATER)
		: nullptr;
	mAnimator->SetTrackImage("Zombie_duckytube", tubeImage);
}

/** 批量设置泳池 reanim 的六条腿部轨道，避免各状态分支遗漏其中一条。 */
void PoolNormalZombie::SetPoolLegsVisible(bool visible) const
{
	if (!mAnimator) return;
	for (const char* track : POOL_LEG_TRACKS) {
		mAnimator->SetTrackVisible(track, visible);
	}
}
