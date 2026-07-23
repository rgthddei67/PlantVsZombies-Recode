#pragma once

#include "Zombie.h"

class ShadowComponent;

/**
 * 泳池普通僵尸：根据身体前后两个探针切换陆地 walk2 与水中 swim，
 * 并为泳池 reanim 使用独立的啃食及死亡帧事件。
 */
class PoolNormalZombie : public Zombie {
public:
	using Zombie::Zombie;

	bool IsInPool() const { return mInPool; }

	void Start() override;
	void ZombieUpdate(float scaledTime) override;
	void HeadDrop() override;
	void ArmDrop() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void ZombieItemUpdate() const override;

protected:
	void SetupZombie() override;
	void RegisterFrameEvents() override;
	const char* GetDeathTrackName() const override;
	void PlayWalkAnimation(float blendTime = 0.0f) override;
	void OnStartEating() override;
	void OnStopEating() override;

private:
	/** 按当前位置同步入水状态；只有状态切换时才换轨道。 */
	void UpdatePoolState();
	/** 同步由入水与啃食状态派生的阴影、腿部和水面泳圈贴图。 */
	void UpdatePoolVisualState() const;
	/** 水中啃食时隐藏双腿并保留带涟漪的泳圈贴图。 */
	void SetPoolEatingVisuals(bool active) const;
	/** 统一设置泳池 reanim 的六条腿部轨道。 */
	void SetPoolLegsVisible(bool visible) const;

	bool mInPool = false;
	ShadowComponent* mPoolShadow = nullptr;  // 基类 Start 创建的非所有权组件缓存，用于入水时隐藏阴影
};
