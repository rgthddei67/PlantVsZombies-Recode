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
	int mBackGround = 0; // ����ͼ
	int mRows = 5;	// ����
	int mColumns = 8; // ����
	int mSun = 50;
	// ����ʾ�У�rows�� �ڲ�columns
	std::vector<std::vector<std::shared_ptr<Cell>>> mCells;

public:
	Board()
	{
		InitializeCell();
	}

	void AddSun(int amount) { mSun += amount; }

	void SubSun(int amount) { mSun -= amount; }

	int GetSun() { return mSun; }

	// ��ʼ������ Ĭ��5��9��
	void InitializeCell(int rows = 4, int cols = 8);

	// ��ȡ��������ָ��
	std::shared_ptr<Cell> GetCell(int row, int col) {
		if (row >= 0 && row < mRows && col >= 0 && col < mColumns) {
			return mCells[row][col];
		}
		return nullptr;
	}

	// ��Ⱦ���񣨵����ã�
	void DrawCell(SDL_Renderer* renderer);

	void Draw(SDL_Renderer* renderer);

	void Update();
};
#endif