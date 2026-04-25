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
	float mCooldownTimer = 0.0f;	// 僵尸减速倒计时时间

	float mCheckPositionTimer = 0.0f;
	bool mIsMindControlled = false;	//有没有被魅惑

	float mEatSoundTimer = 0.0f;
	bool mIsEating = false;
	int mEatPlantID = NULL_PLANT_ID;

	bool mHasHead = true;
	bool mHasArm = true;
	bool mHasTongue = false;
	bool mIsDying = false;	// 是否播放死亡动画 大概可以这么理解 这个时候不能走路
	float mDyingTimer = 0.0f;	// mIsDying 持续时间，超过 10s 强制 Die 防止卡 BUG

	float mSpeed = 10.0f;
	int mGroundTrackIndex = -1;

public:
	Zombie(Board* board, ZombieType zombieType, float x, float y, int row,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);
	~Zombie() = default;

	void Start() override;
	void Update() override;
	virtual void ZombieUpdate(float scaledTime) {}		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	virtual void SaveExtraData(nlohmann::json& j) const {}	// 保存额外数据
	virtual void LoadExtraData(const nlohmann::json& j) {}	// 加载额外数据 
	virtual void ZombieItemUpdate() const; // 处理僵尸读档的时候的手臂、防具等处理
	virtual void Charred();	// 变成灰烬

	int TakeShieldDamage(int damage);
	int TakeHelmDamage(int damage);
	void TakeBodyDamage(int damage);

	int GetSortingKey() const override { return this->mRow; }

	void CheckWin() const;		// 生成奖杯判断

	virtual void ShieldDrop();		// 二类防具掉落 必须调用Zombie::SheildDrop
	virtual void HelmDrop();	// 一类防具掉落 必须调用Zombie::HelmDrop
	virtual void HeadDrop();	// 头掉落 不用调用Zombie::HeadDrop
	virtual void ArmDrop();		// 手掉落 不用调用Zombie::ArmDrop

	virtual void Die();
	virtual void EatTarget(std::shared_ptr<ColliderComponent> other);
	virtual void StopEat(std::shared_ptr<ColliderComponent> other);
	Vector GetVisualPosition() const override;
	Vector GetPosition() const;
	void SetPosition(const Vector& position);

	bool IsMindControlled() const { return this->mIsMindControlled; }
	bool HasHead() const { return this->mHasHead; }
	bool HasArm() const { return this->mHasArm; }
	float GetCooldownTimer() const { return this->mCooldownTimer; }

	void SetCooldown(float timer);		// 设置僵尸减速状态

	void SaveProtectedData(nlohmann::json& j) const;

	void LoadProtectedData(const nlohmann::json& j);

	virtual void ValidateEatingState(EntityManager& em);

protected:
	virtual void ZombieMove(float scaledDelta, TransformComponent* transform);

	// 这才是设置僵尸
	virtual void SetupZombie();

	virtual void CheckHelmImage() {}	// 检查是否应该更换一类防具图片
	virtual void CheckShieldImage() {} 	// 检查是否应该更换二类防具图片
};

#endif