#include "EntityManager.h"
#include "Plant/Plant.h"
#include "Zombie/Zombie.h"
#include "Bullet/Bullet.h"
#include "Coin.h"
#include "LawnMower.h"

int EntityManager::AddPlant(std::shared_ptr<Plant> plant) {
	int id = mNextPlantID++;
	mPlants[id] = plant;
	plant->mPlantID = id;
	return id;
}

Plant* EntityManager::GetPlant(int id) const {
	auto it = mPlants.find(id);
	if (it != mPlants.end())
		return it->second.lock().get();
	return nullptr;
}

std::vector<int> EntityManager::GetAllPlantIDs() const {
	std::vector<int> ids;
	for (const auto& pair : mPlants) {
		if (pair.second.lock())
			ids.push_back(pair.first);
	}
	return ids;
}

int EntityManager::AddZombie(std::shared_ptr<Zombie> zombie) {
	int id = mNextZombieID++;
	mZombies[id] = zombie;
	zombie->mZombieID = id;
	return id;
}

Zombie* EntityManager::GetZombie(int id) const {
	auto it = mZombies.find(id);
	if (it != mZombies.end())
		return it->second.lock().get();
	return nullptr;
}

std::vector<int> EntityManager::GetAllZombieIDs() const {
	std::vector<int> ids;
	for (const auto& pair : mZombies) {
		if (pair.second.lock())
			ids.push_back(pair.first);
	}
	return ids;
}

int EntityManager::AddBullet(std::shared_ptr<Bullet> bullet) {
	int id = mNextBulletID++;
	mBullets[id] = bullet;
	bullet->mBulletID = id;
	return id;
}

Bullet* EntityManager::GetBullet(int id) const {
	auto it = mBullets.find(id);
	if (it != mBullets.end())
		return it->second.lock().get();
	return nullptr;
}

std::vector<int> EntityManager::GetAllBulletIDs() const {
	std::vector<int> ids;
	for (const auto& pair : mBullets) {
		auto bullet = pair.second.lock();
		if (bullet && bullet->IsActive())
			ids.push_back(pair.first);
	}
	return ids;
}

void EntityManager::RemoveBullet(int id) {
	mBullets.erase(id);
}

int EntityManager::AddCoin(std::shared_ptr<Coin> coin) {
	int id = mNextCoinID++;
	mCoins[id] = coin;
	coin->mCoinID = id;
	return id;
}

Coin* EntityManager::GetCoin(int id) const {
	auto it = mCoins.find(id);
	if (it != mCoins.end())
		return it->second.lock().get();
	return nullptr;
}

std::vector<int> EntityManager::GetAllCoinIDs() const {
	std::vector<int> ids;
	for (const auto& pair : mCoins) {
		if (pair.second.lock())
			ids.push_back(pair.first);
	}
	return ids;
}

void EntityManager::EnsureZombieRowIndex() {
	if (!mRowIndexDirty) return;
	// clear() 保留各桶 capacity，跨帧复用，避免反复堆分配。
	for (auto& bucket : mZombiesByRow) bucket.clear();
	for (const auto& pair : mZombies) {
		if (auto z = pair.second.lock()) {
			int row = z->mRow;  // 唯一真相源：换行只改这里，下一帧重建即归位
			if (row >= 0 && row < kMaxRows)
				mZombiesByRow[row].push_back(z.get());
		}
	}
	mRowIndexDirty = false;
}

std::vector<int> EntityManager::CleanupExpired() {
	std::vector<int> removedPlants;

	// 每帧标脏：僵尸的增/删/换行都会改变按行分布，统一靠"下一帧首次查询时重建"兜住，
	// 无需在 Add/Die/换行各处接线（重建只在真的有人 ForEachZombieInRow 查询的帧才触发）。
	mRowIndexDirty = true;

	// 清理植物：每帧扫，返回值用于 Board cell 同步（需准实时）
	for (auto it = mPlants.begin(); it != mPlants.end(); ) {
		if (it->second.expired()) {
			removedPlants.push_back(it->first);
			it = mPlants.erase(it);
		}
		else {
			++it;
		}
	}

	// 僵尸/coin/mower：纯内存 hygiene，攒到 CLEANUP_FULL_INTERVAL 帧一次性扫。
	// 间隔期间过期 weak_ptr 会滞留在 map 里，但 GetZombie/GetCoin 等都通过 weak_ptr.lock()
	// 取值，过期返回 nullptr，调用方语义不受影响。
	if (++mCleanupTick >= CLEANUP_FULL_INTERVAL) {
		mCleanupTick = 0;

		for (auto it = mZombies.begin(); it != mZombies.end(); ) {
			if (it->second.expired()) {
				it = mZombies.erase(it);
			}
			else {
				++it;
			}
		}

		for (auto it = mCoins.begin(); it != mCoins.end(); ) {
			if (it->second.expired()) {
				it = mCoins.erase(it);
			}
			else {
				++it;
			}
		}

		for (auto it = mMowers.begin(); it != mMowers.end(); ) {
			if (it->second.expired()) {
				it = mMowers.erase(it);
			}
			else {
				++it;
			}
		}
	}

	return removedPlants;
}

int EntityManager::AddPlantWithID(std::shared_ptr<Plant> plant, int id) {
	mPlants[id] = plant;
	plant->mPlantID = id;
	if (id >= mNextPlantID) {
		mNextPlantID = id + 1;
	}
	return id;
}

int EntityManager::AddZombieWithID(std::shared_ptr<Zombie> zombie, int id) {
	mZombies[id] = zombie;
	zombie->mZombieID = id;
	if (id >= mNextZombieID) {
		mNextZombieID = id + 1;
	}
	return id;
}

int EntityManager::AddBulletWithID(std::shared_ptr<Bullet> bullet, int id) {
	mBullets[id] = bullet;
	bullet->mBulletID = id;
	if (id >= mNextBulletID) {
		mNextBulletID = id + 1;
	}
	return id;
}

int EntityManager::AddCoinWithID(std::shared_ptr<Coin> coin, int id) {
	mCoins[id] = coin;
	coin->mCoinID = id;
	if (id >= mNextCoinID) {
		mNextCoinID = id + 1;
	}
	return id;
}

int EntityManager::AddMower(std::shared_ptr<Mower> mower) {
	int id = mNextMowerID++;
	mMowers[id] = mower;
	mower->mMowerID = id;
	return id;
}

Mower* EntityManager::GetMower(int id) const {
	auto it = mMowers.find(id);
	if (it != mMowers.end())
		return it->second.lock().get();
	return nullptr;
}

std::vector<int> EntityManager::GetAllMowerIDs() const {
	std::vector<int> ids;
	for (const auto& pair : mMowers) {
		if (pair.second.lock())
			ids.push_back(pair.first);
	}
	return ids;
}

int EntityManager::AddMowerWithID(std::shared_ptr<Mower> mower, int id) {
	mMowers[id] = mower;
	mower->mMowerID = id;
	if (id >= mNextMowerID) {
		mNextMowerID = id + 1;
	}
	return id;
}