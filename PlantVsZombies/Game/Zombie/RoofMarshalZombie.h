#pragma once

#include "Zombie.h"

/**
 * @brief 5-9 屋脊督军；复用普通僵尸骨架，并提供首领耐久与植物直杀抗性。
 */
class RoofMarshalZombie : public Zombie {
public:
	using Zombie::Zombie;

	/** @brief 将所有植物灰烬伤害限制为首领单次伤害上限，土豆雷也不能绕过耐久。 */
	void TakePlantAshDamage(int damage) override;
	/** @brief 大嘴花完成咬合但不能吞下首领，改为结算一次固定基础伤害。 */
	bool TakePlantInstantKill() override;
	/** @brief 首领始终走本体受伤与常规死亡表现，不生成普通僵尸烧焦残影。 */
	bool CanBeCharred() const override { return false; }
	/** @brief 首领不接受魅惑，魅惑菇仍按通用规则被吃掉。 */
	bool CanBeCharmed() const override { return false; }
	/** @brief 缠绕水草只能限时束缚首领，不能把它拖入水下处决。 */
	bool ResistsTangleKelpDrowning() const override { return true; }
	/** @brief 小推车仍会被触发并消耗，但不能借此跳过首领战。 */
	bool CanBeKilledByMower() const override { return false; }

protected:
	void SetupZombie() override;
	/** @brief 隐藏普通头部组并发射军帽与头一体的专属掉落粒子。 */
	void HeadDrop() override;
};
