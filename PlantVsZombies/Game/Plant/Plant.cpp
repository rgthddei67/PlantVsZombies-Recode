#include "Plant.h"
#include "../Board.h"
#include "../Zombie/Zombie.h"
#include "../GameObjectManager.h"
#include "../ShadowComponent.h"
#include "GameDataManager.h"

Plant::Plant(Board* board, PlantType plantType, int row, int column,
	AnimationType animType, float scale, bool isPreview)
	: AnimatedObject(ObjectType::OBJECT_PLANT, board,
		Vector(0, 0), // 位置会在后面计算
		animType,
		ColliderType::BOX,
		Vector(65, 65),
		Vector(-30, -30),
		scale,
		"Plant",
		false)
{
	mBoard = board;
	mPlantType = plantType;
	mRow = row;
	mColumn = column;
	mIsSleeping = false;
	mPlantHealth = 300;
	mPlantMaxHealth = 300;
	mIsPreview = isPreview;

	GameDataManager& plantMgr = GameDataManager::GetInstance();
	Vector plantOffset = plantMgr.GetPlantOffset(plantType);
	// 设置植物在格子中的位置
	if (!mIsPreview) {
		Vector cellCenterPosition(
			CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
			CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2
		);
		SetPosition(cellCenterPosition);  // 逻辑位置
		mVisualOffset = GameDataManager::GetInstance().GetPlantOffset(plantType);
	}
	else {
		SetPosition(Vector(-512, -512));
	}
}

void Plant::SetupPlant()
{

}

void Plant::Start()
{
	GameObject::Start();
	if (this->mIsPreview) {
		RemoveComponent<ColliderComponent>();
	}
	else {
		auto shadowcomponent = AddComponent<ShadowComponent>
			(ResourceManager::GetInstance().GetTexture
			(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
		shadowcomponent->SetDrawOrder(-80);
	}
	this->PlayTrack("anim_idle", 1.0f, 0);
	SetupPlant();
}

void Plant::TakeDamage(int damage) {
	if (mIsPreview) return;
	mPlantHealth -= damage;
	SetGlowingTimer(0.1f);
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

Vector Plant::GetVisualPosition() const {
	return GetTransformComponent()->GetWorldPosition() + mVisualOffset;
}

void Plant::PlantUpdate()
{

}

Vector Plant::GetPosition() const
{
	return GetTransformComponent()->GetWorldPosition();
}

void Plant::SetPosition(const Vector& position)
{
	this->GetTransformComponent()->SetPosition(position);
}
