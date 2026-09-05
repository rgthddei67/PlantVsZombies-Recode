#pragma once
#ifndef _CHERRYBOMB_H
#define _CHERRYBOMB_H

#include "Plant.h"

class CherryBomb : public Plant {
public:
	using Plant::Plant;
	/** 已进入结算动作时禁止搬运，保留原目标及动作提交位置。 */
	bool CanBeRelocated() const override { return false; }

	void SetupPlant() override;

	void TakeDamage(int damage, DamageSource source) override;
	void TakeDeploymentInterceptionDamage(int damage, DamageSource source) override {
		Plant::TakeDamage(damage, source);
	}
	/** 巨人锤击命中充能中的樱桃炸弹时立即爆炸，不生成压扁残影。 */
	void ResolveGargantuarSmash() override;

private:
	/** 统一执行自然到帧与巨人锤击触发的爆炸结算。 */
	void Explode();
};

#endif
