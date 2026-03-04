#include "BulletPool.h"
#include "../Bullet/PeaBullet.h"
#include "../Board.h"
#include "../GameObjectManager.h"
#include <iostream>

void BulletPool::Initialize(int initialCapacity, int warningThreshold) {
	mInitialCapacity = initialCapacity;
	mWarningThreshold = warningThreshold;
	mPool.clear();
	mPool.reserve(initialCapacity);  // 预留初始容量
	mActiveCount = 0;
	mPeakCount = 0;
	mHitCount = 0;
	mMissCount = 0;
}

std::shared_ptr<Bullet> BulletPool::Acquire(Board* board, BulletType type, int row,
	const Vector& colliderRadius, const Vector& position) {

	// 1. 查找空闲对象（相同类型）
	for (auto& pooled : mPool) {
		auto bullet = pooled.bullet.lock();
		if (bullet && !pooled.active && pooled.type == type) {
			// 在重用前，先从 EntityManager 中移除旧 ID 映射
			if (bullet->mBulletID != NULL_BULLET_ID) {
				board->mEntityManager.RemoveBullet(bullet->mBulletID);
			}

			pooled.active = true;
			bullet->SetActive(true);
			bullet->Reset(board, row, colliderRadius, position);
			mActiveCount++;
			if (mActiveCount > mPeakCount) {
				mPeakCount = mActiveCount;
			}
			mHitCount++;
			return bullet;
		}
	}

	// 2. 没有空闲对象，创建新对象并添加到池中（自动扩容）
	PooledBullet pooled;
	pooled.type = type;
	pooled.active = true;

	std::shared_ptr<Bullet> bullet = nullptr;

	// 根据类型创建具体子类
	switch (type) {
	case BulletType::BULLET_PEA:
		bullet = GameObjectManager::GetInstance().CreateGameObjectImmediate<PeaBullet>(
			LAYER_GAME_BULLET,
			board, type, row, colliderRadius, position);
		break;
	default:
		std::cout << "BulletPool::Acquire 未知的子弹类型" << std::endl;
		return nullptr;  
	}

	if (bullet) {
		bullet->SetFromPool(true);  // 标记为来自对象池
		pooled.bullet = bullet;
		mPool.push_back(pooled);  // 直接添加，自动扩容
		mActiveCount++;
		if (mActiveCount > mPeakCount) {
			mPeakCount = mActiveCount;
		}
		mHitCount++;

		// 警告：池大小超过阈值
		if (static_cast<int>(mPool.size()) > mWarningThreshold) {
			std::cout << "警告: BulletPool 大小 " << mPool.size()
				<< " 超过阈值 " << mWarningThreshold << std::endl;
		}

		return bullet;
	}

	return nullptr;
}

void BulletPool::Release(Bullet* bullet) {
	if (!bullet) return;

	// 查找对应的池对象并标记为非活跃
	for (auto& pooled : mPool) {
		auto pooledBullet = pooled.bullet.lock();
		if (pooledBullet && pooledBullet.get() == bullet && pooled.active) {
			pooled.active = false;
			bullet->SetActive(false);
			mActiveCount--;
			return;
		}
	}

	// 如果找不到，说明不是来自对象池的对象（理论上不应该发生）
	std::cout << "警告: BulletPool::Release 找不到对应的池对象" << std::endl;
}

void BulletPool::Clear() {
	// 销毁所有池中的对象
	for (auto& pooled : mPool) {
		auto bullet = pooled.bullet.lock();
		if (bullet) {
			GameObjectManager::GetInstance().DestroyGameObject(bullet);
		}
	}

	mPool.clear();
	mActiveCount = 0;
	mPeakCount = 0;
	mHitCount = 0;
	mMissCount = 0;
}

void BulletPool::PrintStats() const {
	std::cout << "=== BulletPool 统计信息 ===" << std::endl;
	std::cout << "池大小: " << mPool.size() << " (初始容量: " << mInitialCapacity
		<< ", 警告阈值: " << mWarningThreshold << ")" << std::endl;
	std::cout << "活跃对象: " << mActiveCount << std::endl;
	std::cout << "峰值对象: " << mPeakCount << std::endl;
	std::cout << "命中次数: " << mHitCount << std::endl;
	std::cout << "命中率: " << (GetHitRate() * 100.0f) << "%" << std::endl;
}
