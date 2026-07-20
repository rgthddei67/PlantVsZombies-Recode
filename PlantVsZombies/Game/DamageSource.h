#pragma once

// 伤害来源必须由调用方显式声明，防止植物/僵尸增伤词条误作用于另一阵营或环境伤害。
enum class DamageSource {
	PLANT,
	ZOMBIE,
	OTHER,
};
