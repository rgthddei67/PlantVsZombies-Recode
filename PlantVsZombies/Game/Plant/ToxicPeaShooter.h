#pragma once

#include "Shooter.h"

/** 发射可叠加持续毒素的毒豆，射击节奏沿用 Shooter。 */
class ToxicPeaShooter final : public Shooter
{
public:
	using Shooter::Shooter;

protected:
	/** 从随水面视觉位移的炮口锚点生成毒豆。 */
	void ShootBullet() override
	{
		if (!mBoard) return;
		const Vector bulletPosition = GetVisualAnchorPosition() + Vector(30.0f, -30.0f);
		mBoard->CreatePlantBullet(
			BulletType::BULLET_TOXICPEA, mRow, bulletPosition, mPlantType);
	}
};
