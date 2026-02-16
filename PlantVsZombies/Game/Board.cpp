#include "Board.h"
#include "Sun.h"
#include "../GameRandom.h"
#include "./Plant/Plant.h"
#include "./Plant/SunFlower.h"
#include "./Plant/Shooter.h"
#include "./Zombie/Zombie.h"
#include "EntityManager.h"
#include "RenderOrder.h"

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
			auto cell = GameObjectManager::GetInstance().CreateGameObject<Cell>(
				LAYER_BACKGROUND, i, j, position);
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
	// DrawCell(renderer);
}

std::shared_ptr<Sun> Board::CreateSun(const Vector& position, bool needAnimation)
{
	auto sun = GameObjectManager::GetInstance().CreateGameObject<Sun>
		(LAYER_GAME_COIN, this, position, 0.85f, "Sun",
			needAnimation, true);
	if (sun) {
		mEntityManager.AddCoin(sun);
	}

	return sun;
}

std::shared_ptr<Sun> Board::CreateSun(const float& x, const float& y, bool needAnimation) {
	return CreateSun(Vector(x, y), needAnimation);
}

std::shared_ptr<Plant> Board::CreatePlant(PlantType plantType, int row, int column, bool isPreview)
{
	// 检查行列是否有效
	if (row < 0 || row >= mRows || column < 0 || column >= mColumns) {
		std::cout << "无效的行列位置: (" << row << ", " << column << ")" << std::endl;
		return nullptr;
	}

	// 根据植物类型创建对应的植物
	// TODO: 新增植物也要改这里
	std::shared_ptr<Plant> plant = nullptr;

	switch (plantType) {
	case PlantType::PLANT_SUNFLOWER:
		plant = GameObjectManager::GetInstance().CreateGameObjectImmediate<SunFlower>(
			LAYER_GAME_PLANT,        // int renderOrder
			this,                    // Board* board
			PlantType::PLANT_SUNFLOWER,  // PlantType plantType
			row,                    // int row
			column,                 // int column
			AnimationType::ANIM_SUNFLOWER,  // AnimationType animType
			1.0f,                   // scale
			isPreview               // mIsPreview
		);
		break;

	case PlantType::PLANT_PEASHOOTER:
		plant = GameObjectManager::GetInstance().CreateGameObjectImmediate<Shooter>(
			LAYER_GAME_PLANT,
			this,
			PlantType::PLANT_PEASHOOTER,
			row,
			column,
			AnimationType::ANIM_PEASHOOTER,
			1.0f,
			isPreview
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
		mEntityManager.AddPlant(plant);

		// 将植物与格子关联
		auto cell = GetCell(row, column);
		if (cell)
		{
			cell->SetPlantID(plant->mPlantID);
		}
	}

	return plant;
}

std::shared_ptr<Zombie> Board::CreateZombie(ZombieType zombieType, const float& x, int row, bool isPreview) {
	std::shared_ptr<Zombie> zombie = nullptr;

	// 新增僵尸也要改这里
	switch (zombieType) {
	case ZombieType::ZOMBIE_NORMAL:
		zombie = GameObjectManager::GetInstance().CreateGameObjectImmediate<Zombie>(
			LAYER_GAME_ZOMBIE,
			this,
			ZombieType::ZOMBIE_NORMAL,
			x,
			row,
			AnimationType::ANIM_NORMAL_ZOMBIE,
			1.0f,
			isPreview);
		break;
	default:
		std::cout << "未知的僵尸类型" << std::endl;
		return nullptr;
	}

	if (zombie && !isPreview) {
		mEntityManager.AddZombie(zombie);
	}
	return zombie;
}

float Board::RowToY(int row)
{
	return static_cast<float>(CELL_INITALIZE_POS_Y + CELL_COLLIDER_SIZE_Y * 3 / 2 * row);
}

void Board::CleanupExpiredObjects()
{
	// 清理已过期的植物ID映射
	// TODO 如果其他地方也有存储植物ID,也要删除
	std::vector<int> removedPlants = mEntityManager.CleanupExpired();

	// 遍历被清理的植物ID，清除对应Cell中的植物ID
	for (int plantID : removedPlants) {
		CleanPlantFromCells(plantID);
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
		auto sun = CreateSun(sunPos, false);
		sun->StartLinearFall();

		CreateZombie(ZombieType::ZOMBIE_NORMAL, 700, 1, false);
	}
}

void Board::Update()
{
	CleanupExpiredObjects();
	UpdateSunFalling();
}