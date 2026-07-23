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
constexpr float CELL_INITALIZE_POS_X = 242.0f;
constexpr float CELL_INITALIZE_POS_Y = 88.0f;

class Cell : public GameObject {
private:
	std::function<void(int, int)> OnCellClicked;
	ColliderComponent* mCollider = nullptr;
	TransformComponent* mTransform = nullptr;
	Vector mColliderSize = Vector(CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y);
	int mUnderPlantID = NULL_PLANT_ID;
	int mNormalPlantID = NULL_PLANT_ID;

public:
	int mRow = 0;		// 行
	int mColumn = 0;	// 列

	Cell(int row, int column, const Vector& position,
		const Vector& colliderSize = Vector(CELL_COLLIDER_SIZE_X, CELL_COLLIDER_SIZE_Y),
		const std::string& tag = "Cell")
		: mColliderSize(colliderSize), mRow(row), mColumn(column)
	{
		this->mObjectType = ObjectType::OBJECT_NONE;
		SetTag(tag);
		SetName("Cell_" + std::to_string(row) + "_" + std::to_string(column));

		mTransform = this->AddComponent<TransformComponent>(position);
		mCollider = this->AddComponent<ColliderComponent>(mColliderSize);
		mCollider->isTrigger = true;
		mCollider->isStatic = true;
		mCollider->layerMask = CollisionLayer::NONE;
		mCollider->collisionMask = CollisionLayer::NONE;
		auto* clickable = this->AddComponent<ClickableComponent>();
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
		if (mTransform)
		{
			return mTransform->GetPosition();
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
			worldPos.x + mColliderSize.x / 2,
			worldPos.y + mColliderSize.y / 2
		);
	}

	// 设置植物ID
	/** 水面承载层（当前仅睡莲）与普通植物层分开存储，避免叠种时互相覆盖。 */
	void SetUnderPlantID(int plantID) { mUnderPlantID = plantID; }
	void SetNormalPlantID(int plantID) { mNormalPlantID = plantID; }

	// 获取植物ID
	int GetUnderPlantID() const { return mUnderPlantID; }
	int GetNormalPlantID() const { return mNormalPlantID; }
	int GetTopPlantID() const {
		return mNormalPlantID != NULL_PLANT_ID ? mNormalPlantID : mUnderPlantID;
	}

	// 清除植物ID
	void ClearUnderPlantID() { mUnderPlantID = NULL_PLANT_ID; }
	void ClearNormalPlantID() { mNormalPlantID = NULL_PLANT_ID; }

	// 检查格子是否为空
	bool IsEmpty() const
	{
		return mUnderPlantID == NULL_PLANT_ID && mNormalPlantID == NULL_PLANT_ID;
	}

	// 检查点是否在格子内
	bool ContainsPoint(const Vector& point) const
	{
		if (mCollider)
		{
			return mCollider->ContainsPoint(point);
		}
		else
		{
			return false;
		}
	}

	// 获取碰撞体
	ColliderComponent* GetCollider() const
	{
		return mCollider;
	}

	// 设置点击回调
	void SetClickCallback(std::function<void(int, int)> callback)
	{
		OnCellClicked = callback;
	}
};
#endif
