#pragma once
#ifndef _CHERRYBOMB_H
#define _CHERRYBOMB_H

#include "Plant.h"

class CherryBomb : public Plant {
private:
	float mBombTimer = 0.0f;
	float mBombTime = 1.5f;

public:
	using Plant::Plant;

	void SetupPlant() override;
	void PlantUpdate() override;


};

#endif