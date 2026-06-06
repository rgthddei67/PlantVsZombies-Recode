#pragma once
#ifndef _ENTITYMANAGER_H
#define _ENTITYMANAGER_H
#include <memory>
#include <unordered_map>
#include <vector>
#include <array>

class Plant;
class Zombie;
class Coin;
class Bullet;
class Mower;

class EntityManager {
public:
	int AddPlant(std::shared_ptr<Plant> plant);
	Plant* GetPlant(int id) const;
	std::vector<int> GetAllPlantIDs() const;

	int AddZombie(std::shared_ptr<Zombie> zombie);
	Zombie* GetZombie(int id) const;
	std::vector<int> GetAllZombieIDs() const;

	int AddBullet(std::shared_ptr<Bullet> bullet);
	Bullet* GetBullet(int id) const;
	std::vector<int> GetAllBulletIDs() const;
	void RemoveBullet(int id);

	int AddCoin(std::shared_ptr<Coin> coin);
	Coin* GetCoin(int id) const;
	std::vector<int> GetAllCoinIDs() const;

	int AddMower(std::shared_ptr<Mower> mower);
	Mower* GetMower(int id) const;
	std::vector<int> GetAllMowerIDs() const;

	// 带指定 ID 添加实体（用于读档恢复）
	int AddPlantWithID(std::shared_ptr<Plant> plant, int id);
	int AddZombieWithID(std::shared_ptr<Zombie> zombie, int id);
	int AddBulletWithID(std::shared_ptr<Bullet> bullet, int id);
	int AddCoinWithID(std::shared_ptr<Coin> coin, int id);
	int AddMowerWithID(std::shared_ptr<Mower> mower, int id);

	// ID 计数器访问方法（用于存档）
	int GetNextPlantID() const { return mNextPlantID; }
	int GetNextZombieID() const { return mNextZombieID; }
	int GetNextBulletID() const { return mNextBulletID; }
	int GetNextCoinID() const { return mNextCoinID; }
	int GetNextMowerID() const { return mNextMowerID; }

	void SetNextPlantID(int id) { mNextPlantID = id; }
	void SetNextZombieID(int id) { mNextZombieID = id; }
	void SetNextBulletID(int id) { mNextBulletID = id; }
	void SetNextCoinID(int id) { mNextCoinID = id; }
	void SetNextMowerID(int id) { mNextMowerID = id; }

	// 清理过期对象 返回清理的植物ID
	// 植物每帧扫（用于 cell 同步），僵尸/coin/mower 每 CLEANUP_FULL_INTERVAL 帧扫一次（纯内存 hygiene）
	std::vector<int> CleanupExpired();

	// 按行遍历存活僵尸，避免射手/嗅食类植物每次都全表扫描 + 双重 weak_ptr.lock。
	// 索引是"惰性、每帧重建"的（参照 CollisionSystem 的按行分桶）：唯一真相源是僵尸的 mRow。
	// 因此僵尸换行（如后期的大蒜）只需改 mRow，下一帧首次查询时索引自动归位，无需任何额外通知。
	// fn 签名为 void(Zombie*)；只回调本行存活僵尸。
	template<typename Fn>
	void ForEachZombieInRow(int row, Fn&& fn) {
		if (row < 0 || row >= kMaxRows) return;
		EnsureZombieRowIndex();
		for (Zombie* z : mZombiesByRow[row]) fn(z);
	}

private:
	// 每 N 帧做一次"重表"全扫；植物表始终每帧扫
	static constexpr int CLEANUP_FULL_INTERVAL = 30;
	int mCleanupTick = 0;

	int mNextPlantID = 1;
	int mNextZombieID = 1;
	int mNextBulletID = 1;
	int mNextCoinID = 1;
	int mNextMowerID = 1;

	std::unordered_map<int, std::weak_ptr<Plant>> mPlants;
	std::unordered_map<int, std::weak_ptr<Zombie>> mZombies;
	std::unordered_map<int, std::weak_ptr<Bullet>> mBullets;
	std::unordered_map<int, std::weak_ptr<Coin>> mCoins;
	std::unordered_map<int, std::weak_ptr<Mower>> mMowers;

	// ── 僵尸按行空间索引（瞬态、每帧惰性重建）──
	// 与 CollisionSystem::MAX_ROWS 取同值：行号上界，桶用裸指针（删除在 GameObjectManager
	// 里延迟到帧末统一处理，故同帧内裸指针有效，无悬垂）。
	static constexpr int kMaxRows = 8;
	std::array<std::vector<Zombie*>, kMaxRows> mZombiesByRow;
	bool mRowIndexDirty = true;  // 每帧由 CleanupExpired 置脏；首次 ForEachZombieInRow 查询时重建
	void EnsureZombieRowIndex();
};

#endif