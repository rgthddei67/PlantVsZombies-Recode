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
#include "../../GameRandom.h"
#include "../../DeltaTime.h"
#include "../EntityManager.h"

class Board;

class Plant : public AnimatedObject {
public:
	Board* mBoard;
	PlantType mPlantType = PlantType::NUM_PLANT_TYPES;
	Vector mVisualOffset;	// 视觉显示偏移
	int mRow = 0;
	int mColumn = 0;
	int mPlantHealth = 300;
	int mPlantMaxHealth = 300;
	bool mIsSleeping = false;
	bool mIsPreview = false;
	int mPlantID = NULL_PLANT_ID;

public:
	Plant(Board* board, PlantType plantType, int row, int column,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);

	~Plant() = default;
	void Start() override;
	void Update() override;
	Vector GetVisualPosition() const override;
	virtual void PlantUpdate();		// 子类重写Update用这个
	virtual void TakeDamage(int damage);
	void Die();
	Vector GetPosition() const;
	void SetPosition(const Vector& position);
	
protected:
	virtual void SetupPlant();
};

#endif