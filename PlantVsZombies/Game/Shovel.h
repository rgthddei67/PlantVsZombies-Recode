#pragma once
#ifndef _SHOVEL_H
#define _SHOVEL_H

#include "GameObject.h"
#include "Definit.h"
#include "./Plant/Plant.h"

class Board;
struct GLTexture;

enum class ShovelState {
	IDLE,
	ACTIVE,
};

class Shovel : public GameObject {
public:
	Shovel(Board* board);
	void Update() override;
	void Draw(Graphics* g) override;


	void CheckPlant();
	void Activate();
	void SetHomePosition(const Vector& pos);
	void ReturnHome();

	void Die();

private:
	Board* mBoard;
	const GLTexture* mTexture;
	Vector           mPosition;
	Vector           mHomePosition;
	ShovelState      mState;
	std::weak_ptr<Plant> mPlant;	// 选中的植物
};

#endif
