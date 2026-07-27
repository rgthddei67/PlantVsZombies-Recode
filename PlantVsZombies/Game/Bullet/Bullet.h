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
#include "../../Reanimation/Animator.h"

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
	BulletType mPoolType = BulletType::NUM_BULLETS; // 对象池槽位的固定类型；火炬树桩只改变当前表现类型
	int mHitTorchwoodColumn = -1; // 最近处理过本子弹的火炬树桩列，防止同列反复转换
	std::shared_ptr<Animator> mProjectileAnimator;
	bool mAnimatorAdvancedInParallel = false;

	TransformComponent* mTransform = nullptr;
	ColliderComponent* mCollider = nullptr;
	ShadowComponent* mShadow = nullptr;

	// 子弹击中僵尸的效果
	virtual void BulletHitZombie(Zombie* zombie);

	// 按 C# Projectile.DrawShadow 的类型尺寸与棋盘行位置刷新阴影布局。
	void UpdateShadowLayout(const Vector& position);
	// 按当前可变子弹类型重建纹理或 FirePea.reanim 表现。
	void ConfigurePresentation();
	void PlayStandardImpactSound(const Zombie* zombie) const;
	void HitFireballZombie(Zombie* zombie);

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

	void Start() override;
	void Update() override;
	void UpdateParallel(std::vector<DeferredEvent>& outBuf) override;
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
	void SetVelocityX(float x);
	float GetVelocityY() { return mVelocityY; }
	void SetVelocityY(float y) { this->mVelocityY = y; }
	/** 普通豌豆穿过火炬树桩后变为 40 伤害的动画火球。 */
	void ConvertToFireball(int torchwoodColumn);
	/** 寒冰豌豆穿过火炬树桩后退化为普通豌豆；同列不会再被点燃。 */
	void ConvertSnowPeaToPea(int torchwoodColumn);
	int GetHitTorchwoodColumn() const { return mHitTorchwoodColumn; }
	void SetHitTorchwoodColumn(int column) { mHitTorchwoodColumn = column; }
	bool HasAnimatedPresentation() const { return mProjectileAnimator != nullptr; }
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
