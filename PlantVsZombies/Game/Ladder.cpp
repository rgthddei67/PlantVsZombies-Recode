#include "Ladder.h"

#include "Board.h"
#include "Cell.h"
#include "../Graphics.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"

namespace {
	constexpr float kLadderDrawScale = 0.8f; // 原版已放置扶梯的绘制缩放
	constexpr float kLadderOffsetFromCellLeftX = 25.0f; // 原版相对格子左缘的绘制 X 偏移，单位 px
	constexpr float kLadderOffsetFromCellTopY = -4.0f; // 原版相对格子顶边的绘制 Y 偏移，单位 px
}

Ladder::Ladder(Board* board, int row, int column)
	: GameObject(ObjectType::OBJECT_NONE)
	, mRow(row)
	, mColumn(column)
	, mBoard(board)
{
	SetTag("Ladder");
	SetName("Ladder_" + std::to_string(row) + "_" + std::to_string(column));

	const Vector center = mBoard
		? mBoard->GetCellCenterPosition(row, column)
		: Vector(CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + 40.0f,
			CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + 50.0f);
	const float cellHeight = mBoard ? mBoard->GetCellHeight() : CELL_COLLIDER_SIZE_Y;
	mTransform = AddComponent<TransformComponent>(Vector(
		center.x - CELL_COLLIDER_SIZE_X * 0.5f + kLadderOffsetFromCellLeftX,
		center.y - cellHeight * 0.5f + kLadderOffsetFromCellTopY));
}

void Ladder::Draw(Graphics* g)
{
	if (!g || !mTransform) return;
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_ZOMBIE_LADDER_5, false);
	if (!texture) return;
	const Vector position = mTransform->GetPosition();
	g->DrawTexture(texture, position.x, position.y,
		static_cast<float>(texture->width) * kLadderDrawScale,
		static_cast<float>(texture->height) * kLadderDrawScale);
}

Vector Ladder::GetVisualCenter() const
{
	const Vector position = mTransform ? mTransform->GetPosition() : Vector::zero();
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_ZOMBIE_LADDER_5, false);
	if (!texture) return position;
	return position + Vector(
		static_cast<float>(texture->width) * kLadderDrawScale * 0.5f,
		static_cast<float>(texture->height) * kLadderDrawScale * 0.5f);
}
