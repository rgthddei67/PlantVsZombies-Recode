#include "Crater.h"
#include "Board.h"
#include "Cell.h"
#include "GameObjectManager.h"
#include "../GameAPP.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../DeltaTime.h"
#include <algorithm>

Crater::Crater(Board* board, int row, int column, float timeLeft)
	: GameObject(ObjectType::OBJECT_NONE)
	, mRow(row), mColumn(column), mTimeLeft(timeLeft), mBoard(board)
{
	SetTag("Crater");
	SetName("Crater_" + std::to_string(row) + "_" + std::to_string(column));

	// 绘制锚点 = 格子左上 + (-8, +40)，镜像原版 GridItem::DrawCrater 的偏移
	mTransform = AddComponent<TransformComponent>(Vector(
		CELL_INITALIZE_POS_X + column * CELL_COLLIDER_SIZE_X - 8.0f,
		CELL_INITALIZE_POS_Y + row * CELL_COLLIDER_SIZE_Y + 40.0f));
}

void Crater::Update()
{
	GameObject::Update();

	// 选卡期间 GameObjectManager 仍会更新场上对象；弹坑寿命只消耗实际战斗时间。
	if (!mBoard || mBoard->mBoardState != BoardState::GAME) {
		return;
	}

	// 乘 dt：暂停冻结、倍速等比缩放（与原版 GridItemCounter 随游戏更新递减一致）
	mTimeLeft -= DeltaTime::GetDeltaTime();
	if (mTimeLeft <= 0.0f) {
		GameObjectManager::GetInstance().DestroyGameObject(this);
	}
}

void Crater::Draw(Graphics* g)
{
	using namespace ResourceKeys::Textures;

	// 贴图选择镜像原版：黑夜列 1 / 白天列 0；剩余时间过半后换消退版
	const bool night = mBoard
		&& GameAPP::GetInstance().GetBackgroundIsNight(mBoard->mBackGround);
	const bool fading = mTimeLeft < CRATER_DURATION * 0.5f;
	const std::string& key = fading
		? (night ? IMAGE_CRATER_FADING_PART_1 : IMAGE_CRATER_FADING_PART_0)
		: (night ? IMAGE_CRATER_PART_1 : IMAGE_CRATER_PART_0);
	auto tex = ResourceManager::GetInstance().GetTexture(key);
	if (!tex) return;

	float alpha = 255.0f;
	if (mTimeLeft < FADE_OUT_TIME) {
		alpha = 255.0f * std::max(0.0f, mTimeLeft / FADE_OUT_TIME);
	}

	Vector pos = mTransform ? mTransform->GetPosition() : Vector::zero();
	g->DrawTexture(tex, pos.x, pos.y,
		static_cast<float>(tex->width), static_cast<float>(tex->height),
		0.0f, glm::vec4(255.0f, 255.0f, 255.0f, alpha));
}
