#pragma once

#include "MelonPult.h"
#include "../Bullet/BulletType.h"

/** 经典冰瓜：原位升级西瓜投手，并让三行溅射目标减速。 */
class WinterMelon final : public MelonPult
{
public:
	using MelonPult::MelonPult;

protected:
	BulletType GetMelonBulletType() const override
	{
		return BulletType::BULLET_WINTERMELON;
	}
};
