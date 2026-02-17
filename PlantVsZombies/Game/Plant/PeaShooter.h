#pragma once
#ifndef _H_PEASHOOTER_H
#define _H_PEASHOOTER_H

#include "Shooter.h"

class PeaShooter : public Shooter
{
public:
	using Shooter::Shooter;

protected:
	void ShootBullet() override
	{
		if (!mBoard) return;
		Vector bulletPosition = GetPosition() + Vector(30, -30);
		mBoard->CreateBullet(BulletType::BULLET_PEA, mRow, bulletPosition);
	}
};

#endif