#pragma once
#ifndef _CONEZOMBIE_H
#define _CONEZOMBIE_H
#include "Zombie.h"

class ConeZombie : public Zombie {
public:
	using Zombie::Zombie;
	ArmorBrokenState mHelmStage = ArmorBrokenState::NO_BROKEN;
	void HelmDrop() override;

protected:
	void SetupZombie() override;
	void CheckHelmImage() override;
};


#endif