#pragma once

#include "Plant.h"

/**
 * @brief 听雪草：优先迫出本行地下敌人，否则封闭本行雪穴。
 *
 * 两种成功响应共享内部冷却；目标类型和迫出细节由僵尸窄接口自行拥有。
 */
class ListeningGrass final : public Plant {
public:
	using Plant::Plant;

	/** 推进共享冷却，并在就绪边沿尝试一次本行侦听响应。 */
	void PlantUpdate() override;
	/** 保存内部侦听冷却；响应本身在调用边沿已经原子提交。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 钳制恢复内部冷却，不重播音画或再次扫描目标。 */
	void LoadExtraData(const nlohmann::json& j) override;

	float GetListenCooldownRemaining() const { return mListenCooldownRemaining; }

protected:
	/** 配置原版叶子保护伞骨架的待机轨和植物阴影。 */
	void SetupPlant() override;

private:
	/** 按最靠近房屋、稳定 ID 的顺序请求一个敌对目标强制出雪。 */
	bool TryForceSurfaceOne();
	/** 在没有地下目标响应时封闭本行雪穴。 */
	bool TrySealSnowHole();
	/** 播放不承载玩法提交的原版竖叶反馈与植物根部粒子。 */
	void PlayListeningResponse(bool sealedHole, const Vector& effectAnchor);

	float mListenCooldownRemaining = 0.0f;
};
