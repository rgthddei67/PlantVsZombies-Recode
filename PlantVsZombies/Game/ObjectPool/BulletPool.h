#pragma once
#ifndef _BULLETPOOL_H
#define _BULLETPOOL_H

#include <vector>
#include <memory>
#include <unordered_map>
#include "../Bullet/Bullet.h"
#include "../Bullet/BulletType.h"

class Board;

class BulletPool {
private:
	struct PooledBullet {
		std::weak_ptr<Bullet> bullet;  // 使用 weak_ptr 避免循环引用
		bool active = false;
		BulletType type = BulletType::NUM_BULLETS;
	};

	std::vector<PooledBullet> mPool;
	std::vector<std::vector<int>> mFreeByType;         // 各类型空闲槽位下标，Acquire O(1)
	std::unordered_map<Bullet*, int> mBulletIndexMap;  // 指针→下标，Release O(1)
	int mInitialCapacity = 250;   // 初始容量
	int mWarningThreshold = 500;  // 警告阈值

	// 统计信息
	int mActiveCount = 0;
	int mPeakCount = 0;
	int mHitCount = 0;    // 从池中获取的次数
	int mMissCount = 0;   // 未命中次数（保留用于统计）

public:
	BulletPool() = default;
	~BulletPool() = default;

	// 初始化对象池
	void Initialize(int initialCapacity, int warningThreshold = 500);

	// 从池中获取子弹（默认接口，返回原始指针）
	Bullet* Acquire(Board* board, BulletType type, int row,
		const Vector& colliderRadius, const Vector& position);

	// shared_ptr 版本：仅当调用方需要把 weak_ptr 注册到 EntityManager 时使用
	std::shared_ptr<Bullet> AcquireShared(Board* board, BulletType type, int row,
		const Vector& colliderRadius, const Vector& position);

	// 回收子弹到池中
	void Release(Bullet* bullet);

	// 清空对象池
	void Clear();

	// 获取统计信息
	int GetActiveCount() const { return mActiveCount; }
	int GetPeakCount() const { return mPeakCount; }
	int GetHitCount() const { return mHitCount; }
	int GetMissCount() const { return mMissCount; }
	float GetHitRate() const {
		int total = mHitCount + mMissCount;
		return total > 0 ? static_cast<float>(mHitCount) / total : 0.0f;
	}

	// 打印统计信息
	void PrintStats() const;
};

#endif
