#pragma once
#ifndef _ZOMBIE_H
#define _ZOMBIE_H

#include "ZombieType.h"
#include "../AnimatedObject.h"
#include "../Plant/PlantType.h"
#include "../../DeltaTime.h"
#include "../../GameRandom.h"
#include "../EntityManager.h"

class Board;
class Plant;

class Zombie : public AnimatedObject {
public:
	Board* mBoard;
	ZombieType mZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	Vector mVisualOffset;   // 视觉偏移量
	
	int mRow = -1;

	bool mIsPreview = false;
	int mZombieID = NULL_ZOMBIE_ID;

protected:
	float mEatSoundTimer = 0.0f;
	bool mIsEating = false;
	int mEatPlantID = NULL_PLANT_ID;

	bool mHasHead = true;
	bool mHasArm = true;

	float mSpeed = 10.0f;

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
		AnimationType animType, float scale = 1.0f, bool isPreview = false);
	~Zombie() = default;

	void Start() override;
	void Update() override;
	virtual void ZombieUpdate() { }		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	void Die();
	void EatTarget(std::shared_ptr<ColliderComponent> other);
	void StopEat(std::shared_ptr<ColliderComponent> other);
	Vector GetVisualPosition() const override;
	Vector GetPosition() const;
	void SetPosition(const Vector& position);

protected:
	virtual void SetupZombie();
};

#endif