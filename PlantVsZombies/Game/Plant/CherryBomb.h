#pragma once
#ifndef _CHERRYBOMB_H
#define _CHERRYBOMB_H

#include "Plant.h"

class CherryBomb : public Plant {
public:
	using Plant::Plant;

	void SetupPlant() override;
};

#endif