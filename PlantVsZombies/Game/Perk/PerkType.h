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
    PLANT_MIST_REFINING,    // 迷雾燃料单份价值与并发接收上限 +50%/层（最多 2 层）
    PLANT_CORROSIVE_TOXIN,  // 毒液按目标最大可计生命造成百分比伤害（稀有）
    PLANT_UNYIELDING_ROOTS, // 每株植物每轮首次致命伤保留 1 HP 并短暂无敌（稀有）
    PLANT_DAMAGE_ECHO,      // 每 10 次实际植物伤害命中回响一次同额伤害
    ZOMBIE_FOG_BREAKOUT,    // 首次走出浓雾时解控并短暂免控（迷雾专属）
    ZOMBIE_DEATH_RELAY,     // 死亡时给同排前锋传递 1 次免伤
    ZOMBIE_DEVOUR_REPAIR,   // 亲口吃掉植物时修满仍存在的生命层
    ZOMBIE_ARMOR_BREAK_RUSH,// 首次破甲时解控、免控并加速
    COUNT
};

enum class PerkCategory { PLANT_BUFF, ZOMBIE_CURSE };
enum class PerkRarity { COMMON, RARE };
enum class PerkCondition { NONE, PLANTERN_MECHANICS };

struct PerkInfo {
    const char*  key;        // 存档稳定键名（不随 enum 顺序变）
    const char*  nameZh;     // 显示名（UI 用）
    const char*  descZh;     // 每层效果描述（UI 用）
    float        perStack;   // 每层数值
    int          maxStacks;  // 每词条独立上限（=1 即一次性词条）
    PerkCategory category;   // 配对归属：植物增益 / 僵尸增难
    PerkRarity   rarity;     // 抽取稀有度；稀有池按整块面板控制总概率
    PerkCondition condition; // 地图准入条件；与稀有度彼此独立
};

// 一个可选项 = 1 植物增益 + 1 僵尸增难（成对权衡）
struct PerkPairing {
    PerkType plant;
    PerkType zombie;
};

struct PerkOfferContext {
    bool supportsPlanternMechanics = false;
    int rarePanelRollOverride = -1; // -1=正式随机，0=强制普通，1=强制稀有（仅测试）
};
