#pragma once

// 伤害来源必须由调用方显式声明，防止植物/僵尸增伤词条误作用于另一阵营或环境伤害。
enum class DamageSource {
	PLANT,
	PLANT_ASH, // 植物爆炸/灰烬伤害：仍吃植物增伤，但允许僵尸按灰烬类别单独限制
	ZOMBIE,
	OTHER,
};
