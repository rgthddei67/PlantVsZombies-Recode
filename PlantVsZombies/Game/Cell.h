#pragma once
#ifndef _CELL_H
#define _CELL_H
#include "Definit.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"

constexpr float CELL_COLLIDER_SIZE_X = 80.0f;
constexpr float CELL_COLLIDER_SIZE_Y = 100.0f;
constexpr float CELL_INITALIZE_POS_X = 40.0f;
constexpr float CELL_INITALIZE_POS_Y = 80.0f;

class Cell : public GameObject {
public:
	int mRow = 0;		// 行
	int mColumn = 0;	// 列

    Cell(int row, int column, const Vector& position, const std::string& tag = "Cell")
        : mRow(row), mColumn(column)
    {
        SetTag(tag);
        SetName("Cell_" + std::to_string(row) + "_" + std::to_string(column));

        auto transform = this->AddComponent<TransformComponent>(position);

        auto collider = this->AddComponent<ColliderComponent>(
            Vector(CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y), ColliderType::BOX);
        collider->isStatic = true;
        collider->isTrigger = true;
    }
	
	// 获取格子世界位置
	Vector GetWorldPosition() const
	{
		float x = CELL_INITALIZE_POS_X + mColumn * CELL_COLLIDER_SIZE_X;  
		float y = CELL_INITALIZE_POS_Y + mRow * CELL_COLLIDER_SIZE_Y;
		return Vector(x, y);
	}

};
#endif