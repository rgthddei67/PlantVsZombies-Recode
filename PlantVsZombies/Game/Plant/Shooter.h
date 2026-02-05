#pragma once
#ifndef SHOOTER_H
#define SHOOTER_H

#include "Plant.h"
#include "../../DeltaTime.h"

class Shooter : public Plant {
protected:
	float mShootTime = 1.5f;     // 射击间隔时间
	float mShootTimer = 0.0f;    // 射击计时器

	bool HasZombieInRow();		// 检测本行是否有僵尸
	virtual void ShootBullet() { }
	void SetupPlant() override;

public:
	using Plant::Plant;

	void PlantUpdate() override;

};


#endif