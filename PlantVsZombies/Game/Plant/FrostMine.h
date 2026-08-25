#pragma once

#include "Plant.h"

/**
 * 伏霜雷：在准确寒潮预报中校准，并在所在格实际冻结后埋伏首个地面目标。
 */
class FrostMine final : public Plant
{
public:
	enum class Phase {
		DORMANT,
		CALIBRATED,
		ARMED,
		SPENT,
	};

	using Plant::Plant;

	void PlantUpdate() override;
	void OnColdWaveForecastDisrupted() override;
	bool CanBeEaten() const override;
	/** 保存预报校准与已经提交的埋伏状态。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复状态并同步完整三态贴图；旧档默认从未校准开始。 */
	void LoadExtraData(const nlohmann::json& j) override;

	Phase GetPhase() const { return mPhase; }
	const char* GetPhaseName() const;
	bool IsCalibrated() const { return mPhase == Phase::CALIBRATED; }
	bool IsArmed() const { return mPhase == Phase::ARMED; }

protected:
	/** 设置接触触发与三态完整立绘；状态切换不依赖新动画帧事件。 */
	void SetupPlant() override;

private:
	Phase mPhase = Phase::DORMANT;

	/** 按当前阶段切换完整立绘，保持默认与 NoInstance 路径使用同一纹理。 */
	void RefreshPresentation();
	/** 校准状态提交到实际冻土后即永久埋伏，不随本轮回暖回退。 */
	void Arm();
	/** 返回当前与植物碰撞框重叠、稳定 ID 最小的合法地面目标。 */
	class Zombie* FindOverlappingTarget() const;
	/** 原子结算单目标中断、冰层腐蚀、本体伤害和爆裂音画。 */
	void DetonateOn(class Zombie& zombie);
};
