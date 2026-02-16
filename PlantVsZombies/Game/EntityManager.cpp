#include "EntityManager.h"

EntityManager& EntityManager::GetInstance() {
    static EntityManager instance;
    return instance;
}

int EntityManager::AddPlant(std::shared_ptr<Plant> plant) {
    int id = mNextPlantID++;
    mPlants[id] = plant;
    plant->mPlantID = id;
    return id;
}

std::shared_ptr<Plant> EntityManager::GetPlant(int id) const {
    auto it = mPlants.find(id);
    if (it != mPlants.end())
        return it->second.lock();
    return nullptr;
}

std::vector<int> EntityManager::GetAllPlantIDs() const {
    std::vector<int> ids;
    for (const auto& pair : mPlants) {
        if (!pair.second.expired())
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

std::shared_ptr<Zombie> EntityManager::GetZombie(int id) const {
    auto it = mZombies.find(id);
    if (it != mZombies.end())
        return it->second.lock();
    return nullptr;
}

std::vector<int> EntityManager::GetAllZombieIDs() const {
    std::vector<int> ids;
    for (const auto& pair : mZombies) {
        if (!pair.second.expired())
            ids.push_back(pair.first);
    }
    return ids;
}

int EntityManager::AddCoin(std::shared_ptr<Coin> coin) {
    int id = mNextCoinID++;
    mCoins[id] = coin;
    coin->mCoinID = id;
    return id;
}

std::shared_ptr<Coin> EntityManager::GetCoin(int id) const {
    auto it = mCoins.find(id);
    if (it != mCoins.end())
        return it->second.lock();
    return nullptr;
}

std::vector<int> EntityManager::GetAllCoinIDs() const {
    std::vector<int> ids;
    for (const auto& pair : mCoins) {
        if (!pair.second.expired())
            ids.push_back(pair.first);
    }
    return ids;
}

std::vector<int> EntityManager::CleanupExpired() {
    std::vector<int> removedPlants;

    // 清理植物
    for (auto it = mPlants.begin(); it != mPlants.end(); ) {
        if (it->second.expired()) {
            removedPlants.push_back(it->first);
            it = mPlants.erase(it);
        }
        else {
            ++it;
        }
    }

    // 清理僵尸
    for (auto it = mZombies.begin(); it != mZombies.end(); ) {
        if (it->second.expired()) {
            it = mZombies.erase(it);
        }
        else {
            ++it;
        }
    }

    // 清理太阳
    for (auto it = mCoins.begin(); it != mCoins.end(); ) {
        if (it->second.expired()) {
            it = mCoins.erase(it);
        }
        else {
            ++it;
        }
    }

    return removedPlants;
}