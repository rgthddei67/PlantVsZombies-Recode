#include "Plant.h"
#include "../Board.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "PlantDataManager.h"

Plant::Plant(Board* board, PlantType plantType, int row, int column,
	AnimationType animType, const Vector& colliderSize, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_PLANT, board,
		Vector(0, 0), // 位置会在后面计算
		animType,
		ColliderType::BOX,
		colliderSize,
		Vector(0, 0),
		scale,
		"Plant",
		false) // 植物不自动销毁
{
	mBoard = board;
	mPlantType = plantType;
	mRow = row;
	mColumn = column;
	mIsSleeping = false;
	mPlantHealth = 300;
	mPlantMaxHealth = 300;
	mIsPreview = isPreview;

	PlantDataManager& plantMgr = PlantDataManager::GetInstance();
	Vector plantOffset = plantMgr.GetPlantOffset(plantType);
	// 设置植物在格子中的位置
	if (auto transform = mTransform.lock()) {
		if (!mIsPreview) {
			// 计算格子中心位置
			Vector cellCenterPosition(
				CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
				CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2
			);

			Vector plantPosition = cellCenterPosition + plantOffset;

			transform->position = plantPosition;

			mCurrectPosition = cellCenterPosition;
		}
		else {
			transform->position = Vector(-512, -512);
		}
	}
}

void Plant::SetupPlant()
{

}

void Plant::Start()
{
	if (this->mIsPreview) {
		GameObject::Start();
		RemoveComponent<ColliderComponent>();
	}
	else {
		AnimatedObject::Start();
		auto shadowcomponent = AddComponent<ShadowComponent>
			(ResourceManager::GetInstance().GetTexture
			(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
		shadowcomponent->SetDrawOrder(-80);
		SetupPlant();
	}
}

void Plant::TakeDamage(int damage) {
	if (mIsPreview) return;
	mPlantHealth -= damage;
	if (mPlantHealth <= 0) {
		Die();
	}
}

void Plant::Die() {
	StopAnimation();

	// 禁用碰撞体
	if (auto collider = mCollider.lock()) {
		collider->mEnabled = false;
	}

	// 清理植物在Cell上的ID
	if (mBoard) {
		auto cell = mBoard->GetCell(mRow, mColumn);
		if (cell && cell->GetPlantID() == mPlantID) {
			cell->ClearPlantID();
		}
	}
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

void Plant::Update()
{
	AnimatedObject::Update();
	if (!mIsPreview) {
		PlantUpdate();
	}
}

void Plant::PlantUpdate()
{

}
