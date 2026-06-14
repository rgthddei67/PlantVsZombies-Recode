#pragma once

// 生存模式词条类型。新增词条：在此加枚举项（COUNT 之前），并在 SurvivalPerkManager.cpp
// 的 kPerks 表对应位置补一行（static_assert 会强制一一对应）。
enum class PerkType {
    PLANT_DAMAGE_UP,        // 全体植物伤害 +10%/层
    ZOMBIE_HEALTH_UP,       // 僵尸血量 +20%/层
    ZOMBIE_DAMAGE_RESIST,   // 僵尸免伤 +5%/层（最高 50%）
    ZOMBIE_DAMAGE_UP,       // 僵尸对植物伤害 +5%/层（不限层）
    ZOMBIE_INVULN_HITS,     // 僵尸出生后前 10 次受击免伤/层（最多 2 层）
    PLANT_REGEN,            // 植物每 5 秒回 25 HP/层（最多 5 层，满层过量治疗至 3×）
    COUNT
};

struct PerkInfo {
    const char* key;        // 存档稳定键名（不随 enum 顺序变）
    const char* nameZh;     // 显示名（UI 用）
    const char* descZh;     // 每层效果描述（UI 用）
    float       perStack;   // 每层数值（0.10 / 0.20 / 0.05）
    int         maxStacks;  // 每词条独立上限（=1 即一次性词条）
};
