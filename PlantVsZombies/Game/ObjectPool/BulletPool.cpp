#include "BulletPool.h"

#include "../Bullet/PeaBullet.h"
#include "../Bullet/SnowPea.h"

#include "../Board.h"
#include "../GameObjectManager.h"
#include <iostream>

void BulletPool::Initialize(int initialCapacity, int warningThreshold) {
	mInitialCapacity = initialCapacity;
	mWarningThreshold = warningThreshold;
	mPool.clear();
	mPool.reserve(initialCapacity);
	mFreeByType.assign(static_cast<int>(BulletType::NUM_BULLETS), {});
	mBulletIndexMap.clear();
	mActiveCount = 0;
	mPeakCount = 0;
	mHitCount = 0;
	mMissCount = 0;
}

std::shared_ptr<Bullet> BulletPool::AcquireShared(Board* board, BulletType type, int row,
	const Vector& colliderRadius, const Vector& position) {

	int typeIdx = static_cast<int>(type);

	// 1. 从对应类型的空闲列表直接取槽位，O(1)
	auto& freeList = mFreeByType[typeIdx];
	while (!freeList.empty()) {
		int idx = freeList.back();
		freeList.pop_back();
		auto& pooled = mPool[idx];
		auto bullet = pooled.bullet.lock();
		if (bullet) {
			if (bullet->mBulletID != NULL_BULLET_ID) {
				board->mEntityManager.RemoveBullet(bullet->mBulletID);
			}
			pooled.active = true;
			bullet->SetActive(true);
			bullet->Reset(board, row, colliderRadius, position);
			mActiveCount++;
			if (mActiveCount > mPeakCount) mPeakCount = mActiveCount;
			mHitCount++;
			return bullet;
		}
		// weak_ptr 已失效（理论上不应发生），丢弃该槽位
	}

	// 2. 没有空闲对象，创建新对象并添加到池中（自动扩容）
	PooledBullet pooled;
	pooled.type = type;
	pooled.active = true;

	std::shared_ptr<Bullet> bullet = nullptr;

	// TODO: 新增子弹修改我
	switch (type) {
	case BulletType::BULLET_PEA:
		bullet = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<PeaBullet>(
			LAYER_GAME_BULLET,
			board, type, row, colliderRadius, position);
		break;
	case BulletType::BULLET_SNOWPEA:
		bullet = GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<SnowPeaBullet>(
			LAYER_GAME_BULLET,
			board, type, row, colliderRadius, position);
		break;
	default:
		std::cout << "BulletPool::Acquire 未知的子弹类型" << std::endl;
		return nullptr;
	}

	if (bullet) {
		bullet->SetFromPool(true);
		pooled.bullet = bullet;
		int idx = static_cast<int>(mPool.size());
		mPool.push_back(pooled);
		mBulletIndexMap[bullet.get()] = idx;
		mActiveCount++;
		if (mActiveCount > mPeakCount) mPeakCount = mActiveCount;
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

Bullet* BulletPool::Acquire(Board* board, BulletType type, int row,
	const Vector& colliderRadius, const Vector& position) {
	return AcquireShared(board, type, row, colliderRadius, position).get();
}

void BulletPool::Release(Bullet* bullet) {
	if (!bullet) return;

	auto it = mBulletIndexMap.find(bullet);
	if (it == mBulletIndexMap.end()) {
		std::cout << "警告: BulletPool::Release 找不到对应的池对象" << std::endl;
		return;
	}

	int idx = it->second;
	auto& pooled = mPool[idx];
	pooled.active = false;
	bullet->SetActive(false);
	mActiveCount--;
	mFreeByType[static_cast<int>(pooled.type)].push_back(idx);
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
	for (auto& fl : mFreeByType) fl.clear();
	mBulletIndexMap.clear();
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
