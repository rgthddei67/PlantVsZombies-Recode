#pragma once
#ifndef SHOOTER_H
#define SHOOTER_H

#include "Plant.h"
#include "../../DeltaTime.h"

class Shooter : public Plant {
protected:
	std::shared_ptr<Animator> mHeadAnim = nullptr;
	float mCheckZombieTimer = 0.0f;
	float mShootTime = 1.5f;     // 射击间隔时间
	float mShootTimer = 1.0f;    // 射击计时器

	bool HasZombieInRow();		// 检测本行是否有僵尸
	virtual void ShootBullet() = 0;	// 射击子弹 必须写
	void SetupPlant() override;

public:
	using Plant::Plant;

	void SaveExtraData(nlohmann::json& j) const override {
		j["shootTimer"] = mShootTimer;

		// 像主 Animator 一样保存头部动画器的当前轨道与帧
		if (mHeadAnim) {
			j["headAnimTrack"] = mHeadAnim->GetCurrentTrackName();
			j["headAnimFrame"] = mHeadAnim->GetCurrentFrame();
		}
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mShootTimer = j.value("shootTimer", 1.0f);

		// 恢复头部动画器（mHeadAnim 已在 SetupPlant 中创建）
		if (mHeadAnim) {
			std::string headTrack = j.value("headAnimTrack", "");
			if (!headTrack.empty()) {
				mHeadAnim->PlayTrack(headTrack);
				mHeadAnim->SetCurrentFrame(j.value("headAnimFrame", 0.0f));
			}
		}
	}

	void PlantUpdate() override;
};

#endif