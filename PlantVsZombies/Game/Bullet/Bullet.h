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
class ShadowComponent;

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
	const Texture* mTexture = nullptr;
	float mCheckPositionTimer = 0.0f;
	bool mHasHit = false;	// 是否已经击中过僵尸
	int mDamage = 20;			// 子弹伤害
	float mVelocityX = 290.0f;	// 子弹X轴动量
	float mVelocityY = 0.0f;	// 子弹Y轴动量
	bool mThreepeaterMotion = false; // 三线射手斜向豌豆按原版逐步衰减纵向速度

	TransformComponent* mTransform = nullptr;
	ColliderComponent* mCollider = nullptr;
	ShadowComponent* mShadow = nullptr;

	// 子弹击中僵尸的效果
	virtual void BulletHitZombie(Zombie* zombie);

	// 按 C# Projectile.DrawShadow 的类型尺寸与棋盘行位置刷新阴影布局。
	void UpdateShadowLayout(const Vector& position);

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
	// 由 BulletPool 的全局地面阴影阶段调用，保证阴影绘制在植物层之前。
	void DrawShadow(Graphics* g);
	/** 当前类型是否属于会响应台风的轻型植物子弹。 */
	bool IsTyphoonWindAffected() const;
	/** 返回按当前实时风向派生的水平速度；基础速度及存档值保持不变。 */
	float GetWindAdjustedVelocityX() const;
	/** 返回按当前实时风向派生的命中伤害；随后仍由受击入口叠加生存词条。 */
	int GetWindAdjustedDamage() const;

	int GetBulletDamage() { return mDamage; }
	void SetBulletDamage(int damage) { this->mDamage = damage; }
	float GetVelocityX() { return mVelocityX; }
	void SetVelocityX(float x) { this->mVelocityX = x; }
	float GetVelocityY() { return mVelocityY; }
	void SetVelocityY(float y) { this->mVelocityY = y; }
	/**
	 * 启用三线射手斜向轨迹；target row 已由本子弹的 mRow 表示，纵向速度按当前地图行高缩放。
	 * @param sourceRow 发射植物所在行，用于确定初始纵向方向。
	 */
	void EnableThreepeaterMotion(int sourceRow);
	bool IsThreepeaterMotion() const { return mThreepeaterMotion; }

	int GetSortingKey() const override { return this->mRow; }
	TransformComponent* GetTransformComponent() const { return mTransform; }
	Vector GetPosition() const { return GetTransformComponent()->GetPosition(); }
	ColliderComponent* GetColliderComponent() const { return mCollider; }
};

#endif
