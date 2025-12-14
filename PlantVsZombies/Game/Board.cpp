#include "Board.h"
#include "Sun.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/SunFlower.h"

void Board::InitializeCell(int rows, int cols)
{
	mRows = rows + 1;
	mColumns = cols + 1;
	mCells.resize(mRows);
    for (int i = 0; i < mRows; i++)
    {
        mCells[i].resize(mColumns);
        for (int j = 0; j < mColumns; j++)
        {
            Vector position(CELL_INITALIZE_POS_X + j * CELL_COLLIDER_SIZE_X,
                CELL_INITALIZE_POS_Y + i * CELL_COLLIDER_SIZE_Y);
            auto cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(i, j, position);
            mCells[i][j] = cell;
        }
    }
}

void Board::DrawCell(SDL_Renderer* renderer)
{
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 100); // 半透明绿色
    for (auto& row : mCells) {
        for (auto& cell : row) {
            Vector pos = cell->GetWorldPosition();
            SDL_FRect rect = { pos.x, pos.y, CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y };
            SDL_RenderDrawRectF(renderer, &rect);
        }
    }
}

void Board::Draw(SDL_Renderer* renderer)
{
    DrawCell(renderer);
}

std::shared_ptr<Sun> Board::CreateSun(const Vector& position)
{
    auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>(this, position);
    if (sun) {
        sun->mCoinID = mNextCoinID;
        mCoinIDMap[mNextCoinID] = sun;
        mNextCoinID++;
    }

    return sun;
}

std::shared_ptr<Plant> Board::CreatePlant(PlantType plantType, int row, int column, bool isPreview)
{
    // 检查行列是否有效
    if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
        std::cout << "无效的行列位置: (" << row << ", " << column << ")" << std::endl;
        return nullptr;
    }

    // 根据植物类型创建对应的植物
    std::shared_ptr<Plant> plant = nullptr;

    switch (plantType) {
    case PlantType::PLANT_PEASHOOTER:
        break;

    case PlantType::PLANT_SUNFLOWER:
        plant = GameObjectManager::GetInstance().CreateGameObjectImmediate<SunFlower>(
            this,                    // Board* board
            PlantType::PLANT_SUNFLOWER,  // PlantType plantType
            row,                    // int row
            column,                 // int column
            AnimationType::ANIM_SUNFLOWER,  // AnimationType animType
            Vector(60, 60),         // const Vector& colliderSize
            1.0f,                   // scale
			isPreview               // mIsPreview
        );
        break;

    case PlantType::PLANT_WALLNUT:
        break;

    case PlantType::PLANT_CHERRYBOMB:
        break;

    case PlantType::PLANT_SNOWPEA:
        break;

    case PlantType::PLANT_CHOMPER:
        break;

    case PlantType::PLANT_REPEATER:
        break;

    case PlantType::PLANT_POTATOMINE:
        break;

    default:
        std::cout << "未知的植物类型: " << static_cast<int>(plantType) << std::endl;
        break;
    }

    if (plant && !isPreview) {
        plant->mPlantID = mNextPlantID;
        mPlantIDMap[mNextPlantID] = plant;
        mNextPlantID++;

        // 将植物与格子关联
        auto cell = GetCell(row, column);
        if (cell)
        {
			cell->SetPlantID(plant->mPlantID);
        }
    }

    return plant;
}

void Board::CleanupExpiredObjects()
{
    // 清理已过期的植物ID映射
    // TODO 如果其他地方也有存储植物ID,也要删除
    for (auto it = mPlantIDMap.begin(); it != mPlantIDMap.end(); ) {
        if (it->second.expired()) {
            // 清理对应Cell中的植物ID
            int plantID = it->first;
            CleanPlantFromCells(plantID);
            it = mPlantIDMap.erase(it);
        }
        else {
            ++it;
        }
    }

    // 清理已过期的CoinID映射
    for (auto it = mCoinIDMap.begin(); it != mCoinIDMap.end(); ) {
        if (it->second.expired()) {
            it = mCoinIDMap.erase(it);
        }
        else {
            ++it;
        }
    }
}

void Board::CleanPlantFromCells(int plantID)
{
    for (auto& row : mCells) {
        for (auto& cell : row) {
            if (cell->GetPlantID() == plantID) {
                cell->ClearPlantID();
            }
        }
    }
}

void Board::UpdateSunFalling()
{
    mSunCountDown -= DeltaTime::GetDeltaTime();
    if (mSunCountDown <= 0.0f)
    {
        mSunCountDown = 5.0f;
        Vector sunPos(
            GameRandom::Range(50.0f, 770.0f),      // 50~770
            GameRandom::Range(-110.0f, -20.0f)    // -110~-20
        );
        auto sun = CreateSun(sunPos);
        sun->StartLinearFall();
    }
}

void Board::Update()
{
    CleanupExpiredObjects();
    UpdateSunFalling();
}

// 通过ID获取植物
std::shared_ptr<Plant> Board::GetPlantByID(int plantID)
{
    auto it = mPlantIDMap.find(plantID);
    if (it != mPlantIDMap.end()) {
        return it->second.lock();
    }
    return nullptr;
}

// 通过ID获取Coin
std::shared_ptr<Coin> Board::GetCoinByID(int coinID)
{
    auto it = mCoinIDMap.find(coinID);
    if (it != mCoinIDMap.end()) {
        return it->second.lock(); 
    }
    return nullptr;
}

// 获取所有植物的ID
std::vector<int> Board::GetAllPlantIDs() const
{
    std::vector<int> ids;
    for (const auto& pair : mPlantIDMap) {
        if (!pair.second.expired()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}

// 获取所有Coin的ID
std::vector<int> Board::GetAllCoinIDs() const
{
    std::vector<int> ids;
    for (const auto& pair : mCoinIDMap) {
        if (!pair.second.expired()) {
            ids.push_back(pair.first);
        }
    }
    return ids;
}