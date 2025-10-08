#pragma once
#ifndef _BOARD_H
#define _BOARD_H
#include "Sun.h"
#include "Cell.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include <vector>
#include <memory>

class Board {
private:
	int mBackGround = 0; // 背景图
	int mRows = 5;	// 行数
	int mColumns = 8; // 列数
	int mSun = 50;
	// 外层表示行（rows） 内层columns
	std::vector<std::vector<std::shared_ptr<Cell>>> mCells;

public:
	Board()
	{
		InitializeCell();
	}

	void AddSun(int amount) { mSun += amount; }

	void SubSun(int amount) { mSun -= amount; }

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

	// 渲染网格（调试用）
	void DrawCell(SDL_Renderer* renderer);

	void Draw(SDL_Renderer* renderer);

	void Update();
};
#endif