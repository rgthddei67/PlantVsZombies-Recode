#pragma once
#ifndef _BOARD_H
#define _BOARD_H
#include "Cell.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./Plant/PlantType.h"
#include <vector>
#include <memory>

class Sun;
class Coin;
class Plant;
class Zombie;
class Bullet;

constexpr int MAX_SUN = 9990;

class Board {
public:
	int mBackGround = 0; // 背景图
	int mRows = 5;	// 行数
	int mColumns = 8; // 列数
	int mSun = 50;
	float mSunCountDown = 5.0f;
	int mNextPlantID = 1;	// 下一个植物的ID
	int mNextCoinID = 1;	// 下一个Coin的ID
	// 外层表示行（rows） 内层columns
	std::vector<std::vector<std::shared_ptr<Cell>>> mCells;
	std::unordered_map<int, std::weak_ptr<Plant>> mPlantIDMap;
	std::unordered_map<int, std::weak_ptr<Coin>> mCoinIDMap;

public:
	Board()
	{
		InitializeCell();
	}

	void AddSun(int amount) 
	{ 
		int temp = mSun + amount;
		if (temp > MAX_SUN)
		{
			mSun = MAX_SUN;
			return;
		}
		mSun += amount; 
	}

	void SubSun(int amount) 
	{ 
		mSun -= amount;
	}

	int GetSun() { return mSun; }

	// 初始化格子 默认5行9列
	void InitializeCell(int rows = 4, int cols = 8);

	// 获取格子智能指针
	std::shared_ptr<Cell> GetCell(int row, int col) {
		if (row >= 0 && row < mRows && col >= 0 && col < mColumns) {
			return mCells[row][col];
		}
		return nullptr;
	}

	// 创建太阳
	std::shared_ptr<Sun> CreateSun(const Vector& position, bool needAnimation = false);

	// 创建太阳
	std::shared_ptr<Sun> CreateSun(const float& x, const float& y, bool needAnimation = false);

	// 创建植物
	std::shared_ptr<Plant> CreatePlant(PlantType plantType, int row, int column, bool isPreview = false);

	// 渲染网格（调试用）
	void DrawCell(SDL_Renderer* renderer);

	void Draw(SDL_Renderer* renderer);

	// 清理删除的对象
	void CleanupExpiredObjects(); 

	// 从所有Cell中清除指定植物ID
	void CleanPlantFromCells(int plantID);

	void UpdateSunFalling();

	void Update();

	// 根据ID获取植物
	std::shared_ptr<Plant> GetPlantByID(int plantID);

	// 根据ID获取Coin
	std::shared_ptr<Coin> GetCoinByID(int coinID);

	// 获取所有植物的ID
	std::vector<int> GetAllPlantIDs() const;

	// 获取所有Coin的ID
	std::vector<int> GetAllCoinIDs() const;
};
#endif