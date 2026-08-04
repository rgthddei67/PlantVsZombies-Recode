#include "Ladder.h"

#include "Board.h"
#include "Cell.h"
#include "Plant/Plant.h"
#include "../Graphics.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"

namespace {
	constexpr float kLadderDrawScale = 0.8f; // 原版已放置扶梯的绘制缩放
	constexpr float kLadderOffsetFromCellLeftX = 25.0f; // 原版相对格子左缘的绘制 X 偏移，单位 px
	constexpr float kLadderOffsetFromCellTopY = -4.0f; // 原版相对格子顶边的绘制 Y 偏移，单位 px

	// 把当前地图格中心换算为已放置扶梯贴图的左上绘制点。
	Vector LadderDrawPosition(Board* board, int row, int column)
	{
		const Vector center = board
			? board->GetCellCenterPosition(row, column)
			: Vector(CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X + 40.0f,
				CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + 50.0f);
		const float cellHeight = board ? board->GetCellHeight() : CELL_COLLIDER_SIZE_Y;
		return Vector(
			center.x - CELL_COLLIDER_SIZE_X * 0.5f + kLadderOffsetFromCellLeftX,
			center.y - cellHeight * 0.5f + kLadderOffsetFromCellTopY);
	}
}

Ladder::Ladder(Board* board, int row, int column)
	: GameObject(ObjectType::OBJECT_NONE)
	, mRow(row)
	, mColumn(column)
	, mBoard(board)
{
	SetTag("Ladder");
	SetName("Ladder_" + std::to_string(row) + "_" + std::to_string(column));
	mTransform = AddComponent<TransformComponent>(LadderDrawPosition(mBoard, row, column));
}

void Ladder::Draw(Graphics* g)
{
	if (!g || !mTransform) return;
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_ZOMBIE_LADDER_5, false);
	if (!texture) return;
	const Vector position = mTransform->GetPosition() + GetGridMoveVisualOffset();
	g->DrawTexture(texture, position.x, position.y,
		static_cast<float>(texture->width) * kLadderDrawScale,
		static_cast<float>(texture->height) * kLadderDrawScale);
}

void Ladder::MoveToGridCell(int row, int column)
{
	mRow = row;
	mColumn = column;
	SetName("Ladder_" + std::to_string(row) + "_" + std::to_string(column));
	if (mTransform) mTransform->SetPosition(LadderDrawPosition(mBoard, row, column));
}

Vector Ladder::GetGridMoveVisualOffset() const
{
	if (!mBoard) return Vector::zero();
	Plant* host = mBoard->GetTopPlantAt(mRow, mColumn);
	return host && host->IsActive() ? host->GetGridMoveVisualOffset() : Vector::zero();
}

Vector Ladder::GetVisualCenter() const
{
	const Vector position = mTransform
		? mTransform->GetPosition() + GetGridMoveVisualOffset()
		: Vector::zero();
	const Texture* texture = ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_ZOMBIE_LADDER_5, false);
	if (!texture) return position;
	return position + Vector(
		static_cast<float>(texture->width) * kLadderDrawScale * 0.5f,
		static_cast<float>(texture->height) * kLadderDrawScale * 0.5f);
}
