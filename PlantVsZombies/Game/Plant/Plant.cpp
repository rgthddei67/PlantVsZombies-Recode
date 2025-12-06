#include "Plant.h"
#include "../Board.h"
#include "../GameObjectManager.h"

Plant::Plant(Board* board, PlantType plantType, int row, int column,
    AnimationType animType, const Vector& colliderSize, float scale)
    : AnimatedObject(board,
        Vector(0, 0), // 位置会在后面计算
        animType,
        ColliderType::BOX,
        colliderSize,
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
    // 设置植物在格子中的位置
    if (mTransform) {
        Vector plantPosition(
            CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + CELL_COLLIDER_SIZE_X / 2,
            CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + CELL_COLLIDER_SIZE_Y / 2
        );
        mTransform->position = plantPosition;
    }
    SetupPlant();
}

void Plant::SetupPlant()
{
    
}

void Plant::TakeDamage(int damage) {
    mPlantHealth -= damage;
    if (mPlantHealth <= 0) {
        Die();
    }
}

void Plant::Die() {
    // 播放死亡动画
    if (mAnimation) {
		StopAnimation();
    }

    // 禁用碰撞体
    if (mCollider) {
        mCollider->mEnabled = false;
    }
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

void Plant::Update()
{
	AnimatedObject::Update();
}