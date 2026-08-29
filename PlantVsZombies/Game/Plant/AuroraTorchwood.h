#pragma once

#include "Plant.h"

/** 火炬树桩紫卡升级；把指定射手家族的穿越弹丸改写为极光豌豆。 */
class AuroraTorchwood final : public Plant
{
public:
	using Plant::Plant;

	/** 扫描穿过棱晶冠前半部的本行弹丸，并执行最后经过者覆盖转换。 */
	void PlantUpdate() override;
};
