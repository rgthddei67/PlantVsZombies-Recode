#pragma once
#ifndef _ENTITYMANAGER_H
#define _ENTITYMANAGER_H
#include <memory>
#include <unordered_map>
#include <vector>

class Plant;
class Zombie;
class Coin;
class Bullet;

// TODO:存档保存记录mID! 然后再通过id对应到unordered_map
class EntityManager {
public:
    int AddPlant(std::shared_ptr<Plant> plant);
    std::shared_ptr<Plant> GetPlant(int id) const;
    std::vector<int> GetAllPlantIDs() const;

    int AddZombie(std::shared_ptr<Zombie> zombie);
    std::shared_ptr<Zombie> GetZombie(int id) const;
    std::vector<int> GetAllZombieIDs() const;

    int AddBullet(std::shared_ptr<Bullet> bullet);
    std::shared_ptr<Bullet> GetBullet(int id) const;
    std::vector<int> GetAllBulletIDs() const;

    int AddCoin(std::shared_ptr<Coin> coin);
    std::shared_ptr<Coin> GetCoin(int id) const;
    std::vector<int> GetAllCoinIDs() const;

    // 带指定 ID 添加实体（用于读档恢复）
    int AddPlantWithID(std::shared_ptr<Plant> plant, int id);
    int AddZombieWithID(std::shared_ptr<Zombie> zombie, int id);
    int AddBulletWithID(std::shared_ptr<Bullet> bullet, int id);
    int AddCoinWithID(std::shared_ptr<Coin> coin, int id);

    // ID 计数器访问方法（用于存档）
    int GetNextPlantID() const { return mNextPlantID; }
    int GetNextZombieID() const { return mNextZombieID; }
    int GetNextBulletID() const { return mNextBulletID; }
    int GetNextCoinID() const { return mNextCoinID; }

    void SetNextPlantID(int id) { mNextPlantID = id; }
    void SetNextZombieID(int id) { mNextZombieID = id; }
    void SetNextBulletID(int id) { mNextBulletID = id; }
    void SetNextCoinID(int id) { mNextCoinID = id; }

    // 清理过期对象 返回清理的植物ID
    std::vector<int> CleanupExpired();

private:
    int mNextPlantID = 1;
    int mNextZombieID = 1;
    int mNextBulletID = 1;
    int mNextCoinID = 1;

    std::unordered_map<int, std::weak_ptr<Plant>> mPlants;
    std::unordered_map<int, std::weak_ptr<Zombie>> mZombies;
    std::unordered_map<int, std::weak_ptr<Bullet>> mBullets;
    std::unordered_map<int, std::weak_ptr<Coin>> mCoins;
};

#endif