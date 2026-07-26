#pragma once

#include "Plant.h"

/**
 * 经典火爆辣椒：蓄力动画结束时烧毁本行僵尸，并铺开整行火焰表现。
 */
class Jalapeno final : public Plant {
public:
	using Plant::Plant;

	/** 蓄力期间免疫啃食伤害，只保留受击闪光。 */
	void TakeDamage(int damage, DamageSource source) override;

protected:
	/** 切到主人裁定的爆炸轨，并在全局第 19 帧结算。 */
	void SetupPlant() override;

private:
	/** 清除本行寒冷效果、结算灰烬伤害并生成原版十二段火焰。 */
	void IgniteRow();
};
