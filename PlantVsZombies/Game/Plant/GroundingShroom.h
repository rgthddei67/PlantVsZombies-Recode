#pragma once

#include "Shroom.h"

/**
 * @brief 接地菇：替同排三格内植物导走一次黑夜屋顶雷荷，并承受本体反噬。
 */
class GroundingShroom final : public Shroom {
public:
	using Shroom::Shroom;

	bool CanGroundNightRoofChargeFor(const Plant* target) const override;
	bool SuppressesNightRoofChargeProtectionFor(const Zombie* target) const override;
	void AbsorbGroundedNightRoofCharge(bool onWetSlope) override;

protected:
	void SetupPlant() override;
};
