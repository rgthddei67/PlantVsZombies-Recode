#pragma once

#include "Plant.h"

/**
 * 炉芯花：在温暖阶段储存炉芯，并为 3x3 内其他植物拒绝一次冰像封存。
 */
class FurnaceCoreFlower final : public Plant {
public:
	using Plant::Plant;

	/** 仅在实际温度高于冰点且尚未充满时推进炉芯充能。 */
	void PlantUpdate() override;
	/** 消耗一枚炉芯，阻止相邻目标建立本次冰像封存关系。 */
	bool TryPreventIceExecutionSealFor(Plant* target) override;
	/** 保存炉芯数量与未完成的温暖充能进度。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复并修复炉芯状态；读档不得重播充能或保护反馈。 */
	void LoadExtraData(const nlohmann::json& j) override;

	int GetStoredCoreCount() const { return mStoredCores; }
	float GetChargeProgress() const { return mChargeProgress; }
	bool IsCharging() const;
	/** AutoTest 专用：直接建立合法炉芯状态，不触发声音或发光。 */
	void SetCoreStateForTesting(int storedCores, float chargeProgress);

protected:
	/** 复用向日葵时间轴组装木质炉芯、暖色花瓣与两枚状态火焰。 */
	void SetupPlant() override;

private:
	/** 创建稳定命名的炉芯火焰 follower，并配置暖色花瓣。 */
	void ConfigureRig();
	/** 根据权威炉芯数量同步两枚火焰；预览固定展示满充能。 */
	void RefreshPresentation();

	int mStoredCores = 0;
	float mChargeProgress = 0.0f;
};
