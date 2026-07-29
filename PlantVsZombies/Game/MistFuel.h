#pragma once

#include "GameObject.h"
#include "Plant/PlantType.h"

class Board;
struct Texture;

/** 僵尸死亡后飞向路灯花的“雾火”；抵达本体时才把预留量正式计入燃料。 */
class MistFuel : public GameObject {
public:
	MistFuel(Board* board, const Vector& startPosition, int planternID, float amount);

	void Update() override;
	void Draw(Graphics* g) override;

private:
	Board* mBoard = nullptr;
	const Texture* mTexture = nullptr;
	Vector mStartPosition;
	Vector mPosition;
	int mPlanternID = NULL_PLANT_ID;
	float mAmount = 0.0f;
	float mTimer = 0.0f;
};
