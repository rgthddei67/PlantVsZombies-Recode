#pragma once
#ifndef _CELL_H
#define _CELL_H
#include "Definit.h"
#include "ClickableComponent.h"
#include "ColliderComponent.h"
#include "TransformComponent.h"
#include "GameObject.h"
#include "GameObjectManager.h"
#include "./Plant/PlantType.h"

constexpr float CELL_COLLIDER_SIZE_X = 80.0f;
constexpr float CELL_COLLIDER_SIZE_Y = 100.0f;
constexpr float CELL_INITALIZE_POS_X = 40.0f;
constexpr float CELL_INITALIZE_POS_Y = 80.0f;

class Cell : public GameObject {
private:
    std::function<void(int, int)> OnCellClicked;
    std::weak_ptr<ColliderComponent> mCollider;
    std::weak_ptr<TransformComponent> mTransform;
    int mPlantID = NULL_PLANT_ID;

public:
	int mRow = 0;		// 行
	int mColumn = 0;	// 列

    Cell(int row, int column, const Vector& position, const std::string& tag = "Cell")
        : mRow(row), mColumn(column)
    {
		this->mObjectType = ObjectType::OBJECT_NONE;
        SetTag(tag);
        SetName("Cell_" + std::to_string(row) + "_" + std::to_string(column));

        mTransform = this->AddComponent<TransformComponent>(position);
        mCollider = this->AddComponent<ColliderComponent>(
            Vector(CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y));
		auto Collider = mCollider.lock();
        Collider->isTrigger = true;
        Collider->isStatic = true;
        auto clickable = this->AddComponent<ClickableComponent>();
        if (!clickable) return;
        clickable->ChangeCursorOnHover = false;
        clickable->onClick = [this]() {
            if (OnCellClicked) {
                OnCellClicked(mRow, mColumn);
            }
            };
    }
	
	// 获取格子世界位置
    Vector GetWorldPosition() const
    {
        if (auto transform = mTransform.lock()) 
        {
            return transform->position;
        }

        float x = CELL_INITALIZE_POS_X + mColumn * CELL_COLLIDER_SIZE_X;
        float y = CELL_INITALIZE_POS_Y + mRow * CELL_COLLIDER_SIZE_Y;
        return Vector(x, y);
    }

	// 获取格子中心位置
    Vector GetCenterPosition() const
    {
        Vector worldPos = GetWorldPosition();
        return Vector(
            worldPos.x + CELL_COLLIDER_SIZE_X / 2,
            worldPos.y + CELL_COLLIDER_SIZE_Y / 2
        );
    }

    // 设置植物ID
    void SetPlantID(int plantID)
    {
        mPlantID = plantID;
    }

    // 获取植物ID
    int GetPlantID() const
    {
        return mPlantID;
    }

    // 清除植物ID
    void ClearPlantID()
    {
        mPlantID = NULL_PLANT_ID;
    }

    // 检查格子是否为空
    bool IsEmpty() const
    {
        return mPlantID == NULL_PLANT_ID;
    }

    // 检查点是否在格子内
    bool ContainsPoint(const Vector& point) const
    {
        if (auto collider = mCollider.lock())
        {
            return collider->ContainsPoint(point);
        }
        else
        {
            return false;
        }
    }

    // 获取碰撞体
    std::shared_ptr<ColliderComponent> GetCollider() const
    {
        return mCollider.lock();
    }

    // 设置点击回调
    void SetClickCallback(std::function<void(int, int)> callback)
    {
        OnCellClicked = callback;
    }

};
#endif