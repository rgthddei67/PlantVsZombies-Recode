#pragma once
#ifndef _PLANT_H
#define _PLANT_H
#include <iostream>
#include <algorithm>
#include <memory>
#include "./PlantType.h"
#include "../ColliderComponent.h"
#include "../TransformComponent.h"
#include "../AnimatedObject.h"
#include "../AudioSystem.h"

class Board;

class Plant : public AnimatedObject {
public:
	Board* mBoard;
	PlantType mPlantType = PlantType::PLANT_PEASHOOTER;
	int mPlantHealth = 300;
	int mPlantMaxHealth = 300;
	int mRow = 0;
	int mColumn = 0;
	bool mIsSleeping = false;

	Plant(Board* board, PlantType plantType, int row, int column,
		AnimationType animType, const Vector& colliderSize, float scale = 1.0f);

	virtual ~Plant() = default;
	void Update() override;
	void TakeDamage(int damage);
	void Die();
	

protected:
	virtual void SetupPlant();
};

#endif