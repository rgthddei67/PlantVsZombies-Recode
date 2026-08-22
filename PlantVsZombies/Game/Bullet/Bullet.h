#pragma once
#ifndef _BULLET_H
#define _BULLET_H

#include <SDL2/SDL.h>
#include <memory>
#include <vector>
#include "../../DeltaTime.h"
#include "../GameObject.h"
#include "../../GameRandom.h"
#include "../../ResourceManager.h"
#include "../EntityManager.h"
#include "BulletType.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../Transform.h"
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
	float mRotationDegrees = 0.0f; // 星弹等纹理子弹的当前绘制旋转角，单位：度
	float mRotationSpeedDegrees = 0.0f; // 星弹随机自旋速度，单位：度/游戏秒
	bool mThreepeaterMotion = false; // 三线射手斜向豌豆按原版逐步衰减纵向速度
	bool mTargetsFlying = false; // 高姿态仙人掌尖刺为 true；对象池与存档必须显式复位
	bool mLobbedMotion = false; // 解析抛射物状态；对象池复用必须显式清空
	Vector mLobStart = Vector::zero(); // 抛射起点，逻辑弹心坐标
	Vector mLobTarget = Vector::zero(); // 发射时预测并冻结的落点，逻辑弹心坐标
	float mLobElapsed = 0.0f; // 已飞行游戏时间，单位：秒
	float mLobDuration = 0.0f; // 到达预测落点所需游戏时间，单位：秒
	float mLobApexHeight = 0.0f; // 相对起终点连线的最高拱高，单位：像素
	bool mCobCannonMotion = false; // 玉米棒先升空、移到落点上方再垂直落下的专属轨迹
	Vector mCobStart = Vector::zero(); // 玉米加农炮发射点，逻辑弹心坐标
	Vector mCobTarget = Vector::zero(); // 玩家点击后冻结的爆心坐标
	float mCobElapsed = 0.0f; // 玉米棒已飞行游戏时间，单位：秒
	float mCobDuration = 0.0f; // 玉米棒从发射到爆炸的总游戏时间，单位：秒
	int mCobTargetRow = -1; // 点击落点的逻辑行；爆炸严格覆盖上下相邻行
	BulletType mPoolType = BulletType::NUM_BULLETS; // 对象池槽位的固定类型；火炬树桩只改变当前表现类型
	int mHitTorchwoodColumn = -1; // 最近处理过本子弹的火炬树桩列，防止同列反复转换
	std::vector<int> mPiercedZombieIDs; // 尖刺已接触的不同僵尸实体 ID；按玩法穿透上限截断
	std::vector<float> mSpikeDamageRemainders; // 与穿透 ID 对齐的未结算小数伤害额度
	std::shared_ptr<Animator> mProjectileAnimator;
	bool mAnimatorAdvancedInParallel = false;

	// 子弹击中僵尸的效果
	virtual void BulletHitZombie(Zombie* zombie);
	/**
	 * 处理一次子弹与僵尸的碰撞帧；普通弹只消费首次 Enter，尖刺在 Enter/Stay 均结算。
	 */
	void HandleZombieContact(ColliderComponent* other);
	/** 处理投篮车篮球与植物的下降末段碰撞，并按格内投篮层级重定向目标。 */
	void HandlePlantContact(ColliderComponent* other);
	/** 按对象池固定弹型恢复碰撞阵营与回调，避免篮球槽位复用后沿用僵尸掩码。 */
	void ConfigureCollisionTarget();

	// 按 C# Projectile.DrawShadow 的类型尺寸与棋盘行位置刷新阴影布局。
	void UpdateShadowLayout(const Vector& position);
	/** 返回当前弹丸在地形上的阴影基线；屋顶碰撞与阴影绘制必须共用同一采样。 */
	float GetTerrainShadowY(const Vector& position) const;
	/** 按经典弹型离地阈值判断平射弹是否撞上屋顶抬高区域。 */
	bool HitsRoofTerrain(const Vector& position) const;
	/** 播放无目标的地形命中特效并回收弹丸。 */
	void HitRoofTerrain();
	// 按当前可变子弹类型重建纹理或 FirePea.reanim 表现。
	void ConfigurePresentation();
	// 星弹纵向飞行时按当前 Board 网格更新碰撞行；其他子弹保持创建行。
	void UpdateStarRow(const Vector& position);
	/** 播放头盔/护盾材质声，并可抑制无防具时的普通本体 splat。 */
	void PlayStandardImpactSound(
		const Zombie* zombie, bool bypassShield = false, bool includeBodySplat = true) const;
	void HitFireballZombie(Zombie* zombie);
	/** 结算西瓜直击、相邻行溅射和穿透二类护盾的原版语义。 */
	void HitMelonZombie(Zombie* zombie);
	/** 推进解析抛物线；返回 false 表示本帧已落空并回收。 */
	bool UpdateLobbedMotion(float deltaTime);
	/** 抛射物到达无目标落点后的粒子与回收入口。 */
	void HitLobbedGround();
	/** 推进玉米棒升空/换位/垂降三段轨迹；爆炸并回收时返回 false。 */
	bool UpdateCobCannonMotion(float deltaTime);

public:
	Bullet(Board* board, BulletType bulletType, int row, const Vector& colliderRadius,
		const Vector& position);

	// 重置子弹状态（用于对象池复用）
	virtual void Reset(Board* board, int row,
		const Vector& colliderRadius, const Vector& position);

	// 设置是否来自对象池
	void SetFromPool(bool fromPool) { mFromPool = fromPool; }
	bool IsFromPool() const { return mFromPool; }
	BulletType GetPoolType() const { return mPoolType; }

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
	float GetRotationDegrees() const { return mRotationDegrees; }
	float GetRotationSpeedDegrees() const { return mRotationSpeedDegrees; }
	float GetDrawScale() const { return mScale; }
	void SetRotationDegrees(float degrees) { mRotationDegrees = degrees; }
	void SetRotationSpeedDegrees(float degreesPerSecond) {
		mRotationSpeedDegrees = degreesPerSecond;
	}
	/** 普通/毒豆穿过火炬树桩后分别变为普通火豆或紫焰毒火豆。 */
	void ConvertToFireball(int torchwoodColumn);
	/** 寒冰豌豆穿过火炬树桩后退化为普通豌豆；同列不会再被点燃。 */
	void ConvertSnowPeaToPea(int torchwoodColumn);
	/**
	 * @brief 按存档恢复可变子弹类型与火炬树桩防重状态，不改变对象池槽位类型。
	 */
	void RestoreSavedPresentationState(BulletType currentType, int hitTorchwoodColumn);
	int GetHitTorchwoodColumn() const { return mHitTorchwoodColumn; }
	void SetHitTorchwoodColumn(int column) { mHitTorchwoodColumn = column; }
	/** 返回当前类型是否为独立的紫焰毒火豆。 */
	bool IsToxicFireball() const {
		return mBulletType == BulletType::BULLET_TOXICFIREBALL;
	}
	/** 返回尖刺已接触的不同僵尸数量；其他子弹恒为 0。 */
	int GetPiercedZombieCount() const {
		return static_cast<int>(mPiercedZombieIDs.size());
	}
	const std::vector<int>& GetPiercedZombieIDs() const { return mPiercedZombieIDs; }
	const std::vector<float>& GetSpikeDamageRemainders() const {
		return mSpikeDamageRemainders;
	}
	/** 按存档恢复尖刺穿透目标和小数伤害额度；会去重并截断到玩法上限。 */
	void RestorePiercedZombieState(const std::vector<int>& zombieIDs,
		const std::vector<float>& damageRemainders);
	bool HasAnimatedPresentation() const { return mProjectileAnimator != nullptr; }
	/**
	 * 启用三线射手斜向轨迹；target row 已由本子弹的 mRow 表示，纵向速度按当前地图行高缩放。
	 * @param sourceRow 发射植物所在行，用于确定初始纵向方向。
	 */
	void EnableThreepeaterMotion(int sourceRow);
	bool IsThreepeaterMotion() const { return mThreepeaterMotion; }
	/** 设定本弹丸仅命中空中层或地面层目标。 */
	void SetTargetsFlying(bool targetsFlying) { mTargetsFlying = targetsFlying; }
	bool TargetsFlying() const { return mTargetsFlying; }
	float GetTerrainShadowYForTesting() const { return GetTerrainShadowY(GetPosition()); }
	/**
	 * 把当前弹心作为起点，按固定飞行时间和拱高配置解析抛物线。
	 * 目标预测由发射植物负责，本层只保证轨迹精确经过起点和落点。
	 */
	void ConfigureLobbedMotion(
		const Vector& target, float durationSeconds, float apexHeight);
	/** 按存档恢复在途解析抛物线，并重建速度、位置与末段碰撞门禁。 */
	void RestoreLobbedMotion(const Vector& start, const Vector& target,
		float elapsedSeconds, float durationSeconds, float apexHeight);
	bool IsLobbedMotion() const { return mLobbedMotion; }
	const Vector& GetLobStart() const { return mLobStart; }
	const Vector& GetLobTarget() const { return mLobTarget; }
	float GetLobElapsed() const { return mLobElapsed; }
	float GetLobDuration() const { return mLobDuration; }
	float GetLobApexHeight() const { return mLobApexHeight; }
	float GetLobProgress() const;
	float GetLobArcHeight() const;
	/** 配置玉米加农炮专属轨迹；目标由玩家点击冻结，不再追踪实体。 */
	void ConfigureCobCannonMotion(const Vector& target, int targetRow,
		float durationSeconds = 1.4f);
	/** 按存档恢复在途玉米棒；不会重放已经过去的发射音效。 */
	void RestoreCobCannonMotion(const Vector& start, const Vector& target,
		int targetRow, float elapsedSeconds, float durationSeconds);
	bool IsCobCannonMotion() const { return mCobCannonMotion; }
	const Vector& GetCobStart() const { return mCobStart; }
	const Vector& GetCobTarget() const { return mCobTarget; }
	float GetCobElapsed() const { return mCobElapsed; }
	float GetCobDuration() const { return mCobDuration; }
	int GetCobTargetRow() const { return mCobTargetRow; }

	int GetSortingKey() const override { return this->mRow; }
	Vector GetPosition() const { return GetTransform()->GetPosition(); }
	ColliderComponent* GetColliderComponent() { return GetCollider(); }
	const ColliderComponent* GetColliderComponent() const { return GetCollider(); }
};

#endif
