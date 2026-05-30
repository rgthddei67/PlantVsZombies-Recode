#pragma once
#ifndef _H_SHROOM_H
#define _H_SHROOM_H

#include "Plant.h"

class Shroom : public Plant
{
public:
	using Plant::Plant;

protected:
	void SetupPlant() override;
};

#endif