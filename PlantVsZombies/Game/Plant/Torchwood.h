#pragma once
#ifndef _TORCHWOOD_H
#define _TORCHWOOD_H

#include "Plant.h"

class Torchwood final : public Plant
{
public:
	using Plant::Plant;

	/** 扫描穿过树桩前半部的本行豌豆，并执行原版点燃/融冰转换。 */
	void PlantUpdate() override;
};

#endif
