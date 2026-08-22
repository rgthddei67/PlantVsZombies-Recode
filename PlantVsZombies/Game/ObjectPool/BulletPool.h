#pragma once
#ifndef _BULLETPOOL_H
#define _BULLETPOOL_H

#include <vector>
#include <memory>
#include "../Bullet/Bullet.h"
#include "../Bullet/BulletType.h"

class Board;

class BulletPool {
private:
	struct PooledBullet {
		std::shared_ptr<Bullet> bullet;  // 池与 GOM 共同持有；Bullet 不反向持池，不构成循环
		bool active = false;
		BulletType type = BulletType::NUM_BULLETS;
		int activeListIndex = -1;  // 在 mActiveIndices 中的位置，供 O(1) 稠密移除
	};

	std::vector<PooledBullet> mPool;
	std::vector<std::vector<int>> mFreeByType;         // 各类型空闲槽位下标，Acquire O(1)
	std::vector<int> mActiveIndices;                   // 仅保存活跃槽位，绘制成本不受历史峰值影响
	int mInitialCapacity = 250;   // 初始容量
	int mWarningThreshold = 500;  // 警告阈值

	// 统计信息
	int mPeakCount = 0;
	int mHitCount = 0;    // 命中空闲对象并复用的次数
	int mMissCount = 0;   // 无空闲对象、必须新建的次数

	/** 把池槽位加入稠密活跃表，并更新峰值统计。 */
	void ActivateSlot(int poolIndex);

public:
	BulletPool() = default;
	~BulletPool() = default;

	// 初始化对象池
	void Initialize(int initialCapacity, int warningThreshold = 500);

	// 从池中获取子弹（默认接口，返回原始指针）
	Bullet* Acquire(Board* board, BulletType type, int row,
		const Vector& colliderRadius, const Vector& position);

	// shared_ptr 版本：调用方需把弹丸 weak_ptr 注册到 EntityRegistry 时使用
	std::shared_ptr<Bullet> AcquireShared(Board* board, BulletType type, int row,
		const Vector& colliderRadius, const Vector& position);

	// 回收子弹到池中
	void Release(Bullet* bullet);

	// 清空对象池
	void Clear();

	// 在植物等世界对象之前统一绘制所有活跃子弹的地面阴影。
	void DrawShadows(Graphics* g) const;

	// 获取统计信息
	int GetStorageCount() const { return static_cast<int>(mPool.size()); }
	int GetActiveCount() const { return static_cast<int>(mActiveIndices.size()); }
	int GetInactiveCount() const { return GetStorageCount() - GetActiveCount(); }
	int GetPeakCount() const { return mPeakCount; }
	int GetHitCount() const { return mHitCount; }
	int GetMissCount() const { return mMissCount; }
	float GetHitRate() const {
		int total = mHitCount + mMissCount;
		return total > 0 ? static_cast<float>(mHitCount) / total : 0.0f;
	}
	/** 验证池槽位与稠密活跃表的一一对应关系，仅供 AutoTest 状态投影。 */
	bool HasConsistentActiveSlotsForTesting() const;

	// 打印统计信息
	void PrintStats() const;
};

#endif
