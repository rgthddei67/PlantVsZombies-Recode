#include "BulletPool.h"

#include "../Board.h"
#include "../GameObjectManager.h"
#include "../../Logger.h"

namespace {
	// 只接受已有完整表现和碰撞合同的弹型；僵尸豌豆仍是尚未接入工厂的预留枚举。
	bool IsSupportedPooledBulletType(BulletType type)
	{
		switch (type) {
		case BulletType::BULLET_PEA:
		case BulletType::BULLET_SNOWPEA:
		case BulletType::BULLET_CABBAGE:
		case BulletType::BULLET_MELON:
		case BulletType::BULLET_PUFF:
		case BulletType::BULLET_WINTERMELON:
		case BulletType::BULLET_FIREBALL:
		case BulletType::BULLET_STAR:
		case BulletType::BULLET_SPIKE:
		case BulletType::BULLET_BASKETBALL:
		case BulletType::BULLET_KERNEL:
		case BulletType::BULLET_COBBIG:
		case BulletType::BULLET_BUTTER:
		case BulletType::BULLET_TOXICPEA:
		case BulletType::BULLET_TOXICFIREBALL:
			return true;
		case BulletType::BULLET_ZOMBIE_PEA:
		case BulletType::NUM_BULLETS:
			return false;
		}
		return false;
	}
}

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
	if (typeIdx < 0 || typeIdx >= static_cast<int>(mFreeByType.size())
		|| !IsSupportedPooledBulletType(type)) {
		LOG_ERROR("BulletPool") << "Acquire 不支持的子弹类型: " << typeIdx;
		return nullptr;
	}

	// 1. 从对应类型的空闲列表直接取槽位，O(1)
	auto& freeList = mFreeByType[typeIdx];
	while (!freeList.empty()) {
		int idx = freeList.back();
		freeList.pop_back();
		auto& pooled = mPool[idx];
		auto bullet = pooled.bullet.lock();
		if (bullet) {
			if (bullet->mBulletID != NULL_BULLET_ID) {
				board->mEntityRegistry.RemoveBullet(bullet->mBulletID);
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

	// 所有弹型共用同一具体存储类型；差异由 BulletType 与 Reset 合同完整表达。
	std::shared_ptr<Bullet> bullet =
		GameObjectManager::GetInstance().CreateGameObjectImmediateAsShared<Bullet>(
			LAYER_GAME_BULLET,
			board, type, row, colliderRadius, position);

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
			LOG_WARN("BulletPool") << "池大小 " << mPool.size()
				<< " 超过阈值 " << mWarningThreshold;
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
		LOG_WARN("BulletPool") << "Release 找不到对应的池对象";
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

void BulletPool::DrawShadows(Graphics* g) const {
	for (const auto& pooled : mPool) {
		if (!pooled.active) continue;
		if (auto bullet = pooled.bullet.lock()) {
			bullet->DrawShadow(g);
		}
	}
}

void BulletPool::PrintStats() const {
	LOG_DEBUG("BulletPool") << "=== BulletPool 统计信息 ===";
	LOG_DEBUG("BulletPool") << "池大小: " << mPool.size() << " (初始容量: " << mInitialCapacity
		<< ", 警告阈值: " << mWarningThreshold << ")";
	LOG_DEBUG("BulletPool") << "活跃对象: " << mActiveCount;
	LOG_DEBUG("BulletPool") << "峰值对象: " << mPeakCount;
	LOG_DEBUG("BulletPool") << "命中次数: " << mHitCount;
	LOG_DEBUG("BulletPool") << "命中率: " << (GetHitRate() * 100.0f) << "%";
}
