#include "MistFuel.h"

#include "Board.h"
#include "GameObjectManager.h"
#include "Plant/Plantern.h"
#include "../DeltaTime.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"

#include <algorithm>
#include <cmath>

namespace {
	constexpr float kMistFuelFlightSeconds = 0.62f; // 雾火从击杀点飞抵路灯花的纯视觉时长
	constexpr float kMistFuelArcHeight = 58.0f;     // 贝塞尔中点相对直线抬升的像素
	constexpr float kMistFuelDrawSize = 30.0f;      // 128px 原图在战斗场景中的逻辑绘制边长
}

MistFuel::MistFuel(Board* board, const Vector& startPosition, int planternID)
	: GameObject(ObjectType::OBJECT_COIN),
	mBoard(board),
	mTexture(ResourceManager::GetInstance().GetTexture(
		ResourceKeys::Textures::IMAGE_MISTFUEL, false)),
	mStartPosition(startPosition),
	mPosition(startPosition),
	mPlanternID(planternID)
{
	SetName("MistFuel");
}

void MistFuel::Update()
{
	if (!mBoard) {
		GameObjectManager::GetInstance().DestroyGameObject(this);
		return;
	}
	Plantern* plantern = mBoard->GetActivePlantern();
	if (!plantern || plantern->mPlantID != mPlanternID) {
		GameObjectManager::GetInstance().DestroyGameObject(this);
		return;
	}

	mTimer += DeltaTime::GetDeltaTime();
	const float t = std::clamp(mTimer / kMistFuelFlightSeconds, 0.0f, 1.0f);
	const Vector target = plantern->GetVisualAnchorPosition() + Vector(0.0f, -22.0f);
	const Vector control = (mStartPosition + target) * 0.5f + Vector(0.0f, -kMistFuelArcHeight);
	const float oneMinusT = 1.0f - t;
	mPosition = mStartPosition * (oneMinusT * oneMinusT)
		+ control * (2.0f * oneMinusT * t)
		+ target * (t * t);

	if (t >= 1.0f) {
		GameObjectManager::GetInstance().DestroyGameObject(this);
	}
}

void MistFuel::Draw(Graphics* g)
{
	if (!g || !mTexture) return;
	const float pulse = 0.88f + 0.12f * std::sin(mTimer * 18.0f);
	const float size = kMistFuelDrawSize * pulse;
	g->DrawTexture(mTexture,
		mPosition.x - size * 0.5f,
		mPosition.y - size * 0.5f,
		size, size);
}
