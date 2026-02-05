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
	Vector mCurrectPosition = Vector(0, 0);
	int mPlantHealth = 300;
	int mPlantMaxHealth = 300;
	int mRow = 0;
	int mColumn = 0;
	bool mIsSleeping = false;
	bool mIsPreview = false;
	int mPlantID = 0;

	Plant(Board* board, PlantType plantType, int row, int column,
		AnimationType animType, const Vector& colliderSize, float scale = 1.0f, bool isPreview = false);

	virtual ~Plant() = default;
	void Start() override;
	void Update() override;
	virtual void PlantUpdate();		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	void Die();
	
protected:
	virtual void SetupPlant();
};

#endif