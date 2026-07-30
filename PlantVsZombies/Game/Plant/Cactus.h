#pragma once

#include "Plant.h"

/**
 * 发射帧伤尖刺的经典仙人掌。
 *
 * 当前气球僵尸尚未实现，因此只运行低姿态射击；高姿态对接点记录在实现中的 TODO。
 */
class Cactus final : public Plant
{
public:
	using Plant::Plant;

	/** 保存攻击计时器，使读档不会重置下一次射击时点。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复攻击计时器；旧档缺字段时沿用与新实例相同的中性初值。 */
	void LoadExtraData(const nlohmann::json& j) override;
	/** 推进索敌和低姿态射击状态。 */
	void PlantUpdate() override;

protected:
	/** 注册主人指定的第 26 帧低姿态发射事件。 */
	void SetupPlant() override;

private:
	float mCheckZombieTimer = 0.0f;
	float mShootTimer = 1.0f;

	/** 通过行桶查询前方是否存在当前允许索敌的敌对僵尸。 */
	bool HasZombieInRow();
	/** 从稳定视觉锚点发射一枚帧伤尖刺并播放原版 Throw 音效。 */
	void ShootSpike();
};
