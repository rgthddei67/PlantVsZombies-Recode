#pragma once
#ifndef _BOARD_H
#define _BOARD_H
#include "Cell.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include <vector>
#include <memory>

class Sun;
class Coin;

constexpr int MAX_SUN = 9990;

class Board {
private:
	int mBackGround = 0; // 背景图
	int mRows = 5;	// 行数
	int mColumns = 8; // 列数
	int mSun = 50;
	float mSunCountDown = 5.0f;
	// 外层表示行（rows） 内层columns
	std::vector<std::vector<std::shared_ptr<Cell>>> mCells;
	std::vector<std::weak_ptr<Coin>> mCoinObservers;

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
	std::shared_ptr<Sun> CreateSun(const Vector& position);

	// 渲染网格（调试用）
	void DrawCell(SDL_Renderer* renderer);

	void Draw(SDL_Renderer* renderer);

	// 清理删除的对象
	void CleanupExpiredObjects();

	int GetActiveCoinCount() const;

	int GetTotalCreatedCoinCount() const { return static_cast<int>(mCoinObservers.size()); }

	void UpdateSunFalling();

	void Update();
};
#endif