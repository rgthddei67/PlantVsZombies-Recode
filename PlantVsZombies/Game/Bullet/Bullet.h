#pragma once
#ifndef _BULLET_H
#define _BULLET_H

#include <SDL2/SDL.h>
#include <memory>
#include "../../DeltaTime.h"
#include "../GameObject.h"
#include "../../GameRandom.h"
#include "../../ResourceManager.h"
#include "../EntityManager.h"
#include "BulletType.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../TransformComponent.h"
#include "../ColliderComponent.h"
#include "../AudioSystem.h"
#include "../Zombie/Zombie.h"

class Board;
class BulletPool;

class Bullet : public GameObject
{
public:
	BulletType mBulletType = BulletType::NUM_BULLETS;
	float mScale = 0.9f;
	int mRow = -1;
	int mBulletID = NULL_BULLET_ID;
	bool mFromPool = false;  // 标记是否来自对象池

protected:
	Board* mBoard = nullptr;
	const GLTexture* mTexture = nullptr;
	float mCheckPositionTimer = 0.0f;
	bool mHasHit = false;	// 是否已经击中过僵尸
	int mDamage = 20;			// 子弹伤害
	float mVelocityX = 180.0f;	// 子弹X轴动量
	float mVelocityY = 0.0f;	// 子弹Y轴动量

	std::weak_ptr<TransformComponent> mTransform;
	std::weak_ptr<ColliderComponent> mCollider;

	// 子弹击中僵尸的效果
	virtual void BulletHitZombie(std::shared_ptr<Zombie> zombie) { }

public:
	Bullet(Board* board, BulletType bulletType, int row, const Vector& colliderRadius,
		const Vector& position);

	// 重置子弹状态（用于对象池复用）
	virtual void Reset(Board* board, int row,
		const Vector& colliderRadius, const Vector& position);

	// 设置是否来自对象池
	void SetFromPool(bool fromPool) { mFromPool = fromPool; }
	bool IsFromPool() const { return mFromPool; }

	// 子弹消失
	void Die();

	void Update() override;
	void Draw(Graphics* g) override;

	int GetBulletDamage() { return mDamage; }
	void SetBulletDamage(int damage) { this->mDamage = damage; }
	float GetVelocityX() { return mVelocityX; }
	void SetVelocityX(float x) { this->mVelocityX = x; }
	float GetVelocityY() { return mVelocityY; }
	void SetVelocityY(float y) { this->mVelocityY = y; }

	std::shared_ptr<TransformComponent> GetTransformComponent() const { return mTransform.lock(); }
	Vector GetPosition() const { return GetTransformComponent()->GetPosition(); }
	std::shared_ptr<ColliderComponent> GetColliderComponent() const { return mCollider.lock(); }
};

#endif