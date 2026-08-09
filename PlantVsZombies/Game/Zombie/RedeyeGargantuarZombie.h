#pragma once

#include "GargantuarZombie.h"

/** 红眼巨人僵尸：完整复用经典巨人行为，仅将本体生命提高到 6000 并替换红眼头部。 */
class RedeyeGargantuarZombie final : public GargantuarZombie {
public:
	using GargantuarZombie::GargantuarZombie;

protected:
	void SetupZombie() override;
	const std::string& GetHeadTextureKey(int damageStage) const override;
};
