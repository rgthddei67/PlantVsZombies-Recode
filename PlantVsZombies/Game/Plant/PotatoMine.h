#pragma once
#ifndef _POTATO_MINE_H
#define _POTATO_MINE_H

#include "Plant.h"

class PotatoMine : public Plant {
private:
	float mReadyTimer = 0.0f;	// 准备的时间
	float mBoomTimer = 0.0f;	// 爆炸后的时间
	bool mIsBoom = false;		// 是否已经爆炸
	bool mIsRise = false;		// 是否升起

public:
	using Plant::Plant;

	void PlantUpdate() override;

	void SaveExtraData(nlohmann::json& j) const override;

	void LoadExtraData(const nlohmann::json& j) override;

protected:
	void SetupPlant() override;

	/** 进入压扁态并按原版半径一次结算同排范围内的僵尸。 */
	void Detonate();

	/** 清除爆炸圆与僵尸判定矩形相交的所有非魅惑目标。 */
	void KillZombiesInBlastRadius();

	void Ready(bool quick);
};

#endif
