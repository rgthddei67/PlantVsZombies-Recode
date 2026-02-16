#pragma once
#ifndef _ZOMBIE_H
#define _ZOMBIE_H

#include "ZombieType.h"
#include "../AnimatedObject.h"
#include "../Plant/PlantType.h"
#include "../../DeltaTime.h"
#include "../../GameRandom.h"

class Board;
class Plant;

class Zombie : public AnimatedObject {
public:
	Board* mBoard;
	ZombieType mZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	Vector mCurrectPosition = Vector(0, 0);
	
	int mRow = -1;

	bool mIsPreview = false;
	int mZombieID = NULL_ZOMBIE_ID;

protected:
	bool mIsEating = false;
	int mEatPlantID = NULL_PLANT_ID;

	bool mHasHead = true;
	bool mHasArm = true;

	int mBodyHealth = 270;
	int mBodyMaxHealth = 270;
	HelmType mHelmType = HelmType::HELMTYPE_NONE;
	int mHelmHealth = 0;
	int mHelmMaxHealth = 0;
	ShieldType mShieldType = ShieldType::SHIELDTYPE_NONE;
	int mShieldHealth = 0;
	int mShieldMaxHealth = 0;

public:
	Zombie(Board* board, ZombieType zombieType, float x, int row,
		AnimationType animType, const Vector& colliderSize, float scale = 1.0f, bool isPreview = false);
	~Zombie() = default;

	void Start() override;
	void Update() override;
	virtual void ZombieUpdate() { }		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	void Die();

protected:
	virtual void SetupZombie();
};

#endif