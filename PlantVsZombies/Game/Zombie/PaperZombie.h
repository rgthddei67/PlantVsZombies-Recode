#pragma once
#ifndef _PARERZOMBIE_H_
#define _PARERZOMBIE_H_

#include "Zombie.h"

class PaperZombie : public Zombie {
protected:
	bool mHasNewspaper = true;	// 是否有报纸
	ArmorBrokenState mShieldStage = ArmorBrokenState::NO_BROKEN;	

public:
	using Zombie::Zombie;

};

#endif