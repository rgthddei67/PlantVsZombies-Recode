#pragma once
#ifndef _LILYPAD_H
#define _LILYPAD_H

#include "Plant.h"

/**
 * 睡莲占据水格的下层，为同格普通植物提供种植基础。
 * 本类只维护原版种下后一秒的啃咬保护；分层占格与水面浮动由 Board/Plant 公共路径处理。
 */
class LilyPad : public Plant
{
private:
	float mBiteProtectionTimer = 1.0f;	// 原版 LilypadInvulnerable 的 100cs，仅阻止僵尸啃咬伤害

public:
	using Plant::Plant;

	void PlantUpdate() override;

	/** 保护倒计时结束后才允许僵尸啃咬结算。 */
	bool CanBeEaten() const override;
	bool IsBiteProtected() const { return mBiteProtectionTimer > 0.0f; }

	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

protected:
	/** 移除陆生植物通用阴影；睡莲原版不绘制独立阴影。 */
	void SetupPlant() override;
};

#endif
