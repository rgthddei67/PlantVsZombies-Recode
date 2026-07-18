#pragma once

// 生存模式词条类型。新增词条：在此加枚举项（COUNT 之前），并在 SurvivalPerkManager.cpp
// 的 kPerks 表对应位置补一行（static_assert 会强制一一对应）。
enum class PerkType {
    PLANT_DAMAGE_UP,        // 全体植物伤害 +12%/层
    ZOMBIE_HEALTH_UP,       // 僵尸血量 +12%/层
    ZOMBIE_DAMAGE_RESIST,   // 僵尸免伤 +3%/层（最多 15 层）
    ZOMBIE_DAMAGE_UP,       // 僵尸对植物伤害 +5%/层（不限层）
    ZOMBIE_INVULN_HITS,     // 僵尸出生后前 4 次受击免伤/层（最多 2 层）
    PLANT_REGEN,            // 植物每 5 秒回 65 HP/层（最多 8 层，5 层解锁过量治疗至 3×）
    PLANT_ATTACK_SPEED,     // 全体植物开火速度 +15%/层（最多 8 层）
    PLANT_DAMAGE_REDUCTION, // 全体植物受到伤害 -3%/层（最多 15 层）
    PLANT_SUN_BONUS,        // 收集阳光 +15%/层（最多 10 层）
    PLANT_CARD_RECHARGE,    // 卡片冷却速度 +12%/层（最多 10 层）
    COUNT
};

enum class PerkCategory { PLANT_BUFF, ZOMBIE_CURSE };

struct PerkInfo {
    const char*  key;        // 存档稳定键名（不随 enum 顺序变）
    const char*  nameZh;     // 显示名（UI 用）
    const char*  descZh;     // 每层效果描述（UI 用）
    float        perStack;   // 每层数值
    int          maxStacks;  // 每词条独立上限（=1 即一次性词条）
    PerkCategory category;   // 配对归属：植物增益 / 僵尸增难
};

// 一个可选项 = 1 植物增益 + 1 僵尸增难（成对权衡）
struct PerkPairing {
    PerkType plant;
    PerkType zombie;
};
