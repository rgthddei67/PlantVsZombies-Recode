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

	void PlantUpdate() override;

};


#endif