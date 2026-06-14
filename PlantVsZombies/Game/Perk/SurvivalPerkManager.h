#pragma once
#include <array>
#include <nlohmann/json_fwd.hpp>   // 仅前置声明 nlohmann::json，Save/Load 取引用参数足够
#include "PerkType.h"

// 生存模式词条聚合管理器。Board 以值成员持有；非生存关恒空，聚合 getter 返回中性值。
class SurvivalPerkManager {
public:
    bool AddPerk(PerkType type);          // +1 层，到 maxStacks 截断；已满返回 false
    int  GetStacks(PerkType type) const;
    void Clear();                         // 进新生存局时清空

    // 聚合效果——空词条时为中性值（乘法单位元 / 原值），三处钟点自动 no-op
    double GetPlantDamageMultiplier() const;       // 1 + 0.10 * stacks
    double GetZombieHealthMultiplier() const;      // 1 + 0.20 * stacks
    double GetZombieDamageTakenMultiplier() const; // 1 - 0.05 * stacks，夹底 0.5

    int ScalePlantDamage(int base) const;          // round(base * 植物伤害倍率)，base>=1 时结果>=1
    int ScaleDamageToZombie(int base) const;       // round(base * 免伤倍率)，base>=1 时结果>=1

    int   ScaleZombieDamage(int base) const;        // 词条①：round(base*(1+0.05*stacks))，base>=1→>=1
    int   GetZombieInvulnHits() const;              // 词条②：10*stacks（无层→0）
    float GetPlantRegenInterval() const;            // 词条③：固定 5.0 秒
    int   GetPlantRegenPerPulse() const;            // 词条③：25*stacks（无层→0）
    int   GetPlantRegenHpCap(int maxHealth) const;  // 词条③：满层→maxHealth*3，否则 maxHealth

    static const PerkInfo& GetInfo(PerkType type); // 静态元数据表（UI 用）

    void Save(nlohmann::json& j) const;            // 仅写 stacks>0 的项，按 key 字符串
    void Load(const nlohmann::json& j);            // 缺 key→0；越界→夹回 [0,maxStacks]

private:
    std::array<int, static_cast<size_t>(PerkType::COUNT)> mStacks{};  // 全 0 初始
};
