#pragma once

#include "GameObject.h"
#include "Plant/PlantType.h"

class Board;
struct Texture;

/** 僵尸死亡后飞向路灯花的纯视觉“雾火”；燃料已在生成时结算，本对象不进存档。 */
class MistFuel : public GameObject {
public:
	MistFuel(Board* board, const Vector& startPosition, int planternID);

	void Update() override;
	void Draw(Graphics* g) override;

private:
	Board* mBoard = nullptr;
	const Texture* mTexture = nullptr;
	Vector mStartPosition;
	Vector mPosition;
	int mPlanternID = NULL_PLANT_ID;
	float mTimer = 0.0f;
};
