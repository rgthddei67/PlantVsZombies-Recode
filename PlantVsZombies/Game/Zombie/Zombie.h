#pragma once
#ifndef _ZOMBIE_H
#define _ZOMBIE_H

#include "ZombieType.h"
#include "../AnimatedObject.h"
#include "../Plant/PlantType.h"
#include "../../DeltaTime.h"
#include "../../GameRandom.h"
#include "../EntityManager.h"
#include <nlohmann/json.hpp>

class Board;
class Plant;

class Zombie : public AnimatedObject {
public:
	Board* mBoard;
	ZombieType mZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	Vector mVisualOffset;   // 视觉偏移量

	int mRow = -1;

	int mAttackDamage = 50;

	bool mIsPreview = false;
	bool mNeedDropArm = true;
	bool mNeedDropHead = true;
	int mZombieID = NULL_ZOMBIE_ID;

	int mSpawnWave = -1;	// 多少波刷新的

	int mBodyHealth = 270;
	int mBodyMaxHealth = 270;
	HelmType mHelmType = HelmType::HELMTYPE_NONE;
	int mHelmHealth = 0;
	int mHelmMaxHealth = 0;
	ShieldType mShieldType = ShieldType::SHIELDTYPE_NONE;
	int mShieldHealth = 0;
	int mShieldMaxHealth = 0;

protected:
	float mCheckPositionTimer = 0.0f;
	bool mIsMindControlled = false;	//有没有被魅惑

	float mEatSoundTimer = 0.0f;
	bool mIsEating = false;
	int mEatPlantID = NULL_PLANT_ID;

	bool mHasHead = true;
	bool mHasArm = true;
	bool mIsDying = false;	// 是否播放死亡动画 大概可以这么理解 这个时候不能走路

	float mSpeed = 10.0f;

public:
	Zombie(Board* board, ZombieType zombieType, float x, float y, int row,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);
	~Zombie() = default;

	void Start() override;
	void Update() override;
	virtual void ZombieUpdate() {}		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	virtual void SaveExtraData(nlohmann::json& j) const {}	// 保存额外数据
	virtual void LoadExtraData(const nlohmann::json& j) {}	// 加载额外数据 
	virtual void ZombieItemUpdate() const; // 处理僵尸防具等内容
	int TakeShieldDamage(int damage);
	int TakeHelmDamage(int damage);
	void TakeBodyDamage(int damage);

	int GetSortingKey() const override { return mRow; }

	virtual void ShieldDrop();		// 二类防具掉落 必须调用Zombie::SheildDrop
	virtual void HelmDrop();	// 一类防具掉落 必须调用Zombie::HelmDrop
	virtual void HeadDrop();	// 头掉落 不用调用Zombie::HeadDrop
	virtual void ArmDrop();		// 手掉落 不用调用Zombie::ArmDrop

	void Die();
	void EatTarget(std::shared_ptr<ColliderComponent> other);
	void StopEat(std::shared_ptr<ColliderComponent> other);
	Vector GetVisualPosition() const override;
	Vector GetPosition() const;
	void SetPosition(const Vector& position);

	bool IsMindControlled() { return this->mIsMindControlled; }

	void SaveProtectedData(nlohmann::json& j) const {
		j["isMindControlled"] = mIsMindControlled;
		j["isEating"] = mIsEating;
		j["eatPlantID"] = mEatPlantID;
		j["hasHead"] = mHasHead;
		j["hasArm"] = mHasArm;
		j["isDying"] = mIsDying;
		j["speed"] = mSpeed;
	}

	void LoadProtectedData(const nlohmann::json& j) {
		mIsMindControlled = j.value("isMindControlled", false);
		mIsEating = j.value("isEating", false);
		mEatPlantID = j.value("eatPlantID", NULL_PLANT_ID);
		mHasHead = j.value("hasHead", true);
		mHasArm = j.value("hasArm", true);
		mIsDying = j.value("isDying", false);
		mSpeed = j.value("speed", 10.0f);
	}

protected:
	// 此处仅用于设置僵尸死亡的回调函数! 不要在子类中调用基类！
	virtual void SetupZombieDeathEvent();

	// 这才是设置僵尸
	virtual void SetupZombie() {}

	virtual void CheckHelmImage() {}	// 检查是否应该更换一类防具图片
	virtual void CheckShieldImage() {} 	// 检查是否应该更换二类防具图片
};

#endif