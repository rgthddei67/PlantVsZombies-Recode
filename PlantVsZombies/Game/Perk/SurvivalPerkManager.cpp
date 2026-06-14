#include "SurvivalPerkManager.h"
#include <nlohmann/json.hpp>

namespace {
    // 顺序必须与 PerkType 枚举一一对应（static_assert 强制）
    const PerkInfo kPerks[] = {
        { "PLANT_DAMAGE_UP",      u8"全体植物伤害", u8"每层使全体植物伤害 +10%",            0.10f, 10 },
        { "ZOMBIE_HEALTH_UP",     u8"僵尸血量",     u8"每层使僵尸血量 +20%",                0.20f, 10 },
        { "ZOMBIE_DAMAGE_RESIST", u8"僵尸免伤",     u8"每层使僵尸受到伤害 -5%（最高 50%）", 0.05f, 10 },
        { "ZOMBIE_DAMAGE_UP",     u8"僵尸伤害",     u8"每层使僵尸对植物伤害 +5%（不限层）", 0.05f, 9999 },
        { "ZOMBIE_INVULN_HITS",   u8"僵尸前N次免伤", u8"每层使僵尸出生后前 10 次受击免伤（最多 2 层）", 10.0f, 2 },
        { "PLANT_REGEN",          u8"植物回血",     u8"每 5 秒回 25 HP；满 5 层解锁过量治疗至 3 倍上限", 25.0f, 5 },
    };
    static_assert(sizeof(kPerks) / sizeof(kPerks[0]) == static_cast<size_t>(PerkType::COUNT),
                  "kPerks 必须与 PerkType 一一对应");

    constexpr float kPlantRegenIntervalSec  = 5.0f;   // 词条③回血间隔
    constexpr int   kPlantRegenOverhealMult = 3;      // 词条③满层过量治疗倍率（封顶 maxHealth*3）

    int RoundScale(int base, double mult) {
        if (base <= 0) return base;
        int r = static_cast<int>(static_cast<double>(base) * mult + 0.5);
        return r < 1 ? 1 : r;   // 防 50% 免伤把 1 点伤害抹成 0
    }
}

const PerkInfo& SurvivalPerkManager::GetInfo(PerkType type) {
    return kPerks[static_cast<size_t>(type)];
}

bool SurvivalPerkManager::AddPerk(PerkType type) {
    int& s = mStacks[static_cast<size_t>(type)];
    if (s >= GetInfo(type).maxStacks) return false;
    ++s;
    return true;
}

int SurvivalPerkManager::GetStacks(PerkType type) const {
    return mStacks[static_cast<size_t>(type)];
}

void SurvivalPerkManager::Clear() {
    mStacks.fill(0);
}

double SurvivalPerkManager::GetPlantDamageMultiplier() const {
    return 1.0 + GetInfo(PerkType::PLANT_DAMAGE_UP).perStack
               * GetStacks(PerkType::PLANT_DAMAGE_UP);
}

double SurvivalPerkManager::GetZombieHealthMultiplier() const {
    return 1.0 + GetInfo(PerkType::ZOMBIE_HEALTH_UP).perStack
               * GetStacks(PerkType::ZOMBIE_HEALTH_UP);
}

double SurvivalPerkManager::GetZombieDamageTakenMultiplier() const {
    double reduction = GetInfo(PerkType::ZOMBIE_DAMAGE_RESIST).perStack
                     * GetStacks(PerkType::ZOMBIE_DAMAGE_RESIST);
    double mult = 1.0 - reduction;
    return mult < 0.5 ? 0.5 : mult;   // maxStacks=10 → 恰好 0.5；夹底防越界
}

int SurvivalPerkManager::ScalePlantDamage(int base) const {
    return RoundScale(base, GetPlantDamageMultiplier());
}

int SurvivalPerkManager::ScaleDamageToZombie(int base) const {
    return RoundScale(base, GetZombieDamageTakenMultiplier());
}

void SurvivalPerkManager::Save(nlohmann::json& j) const {
    for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
        if (mStacks[i] > 0) j[kPerks[i].key] = mStacks[i];
    }
}

void SurvivalPerkManager::Load(const nlohmann::json& j) {
    for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
        int v = j.value(kPerks[i].key, 0);
        if (v < 0) v = 0;
        if (v > kPerks[i].maxStacks) v = kPerks[i].maxStacks;
        mStacks[i] = v;
    }
}

int SurvivalPerkManager::ScaleZombieDamage(int base) const {
    return RoundScale(base, 1.0 + GetInfo(PerkType::ZOMBIE_DAMAGE_UP).perStack
                                 * GetStacks(PerkType::ZOMBIE_DAMAGE_UP));
}

int SurvivalPerkManager::GetZombieInvulnHits() const {
    return static_cast<int>(GetInfo(PerkType::ZOMBIE_INVULN_HITS).perStack
                            * GetStacks(PerkType::ZOMBIE_INVULN_HITS) + 0.5f);
}

float SurvivalPerkManager::GetPlantRegenInterval() const {
    return kPlantRegenIntervalSec;
}

int SurvivalPerkManager::GetPlantRegenPerPulse() const {
    return static_cast<int>(GetInfo(PerkType::PLANT_REGEN).perStack
                            * GetStacks(PerkType::PLANT_REGEN) + 0.5f);
}

int SurvivalPerkManager::GetPlantRegenHpCap(int maxHealth) const {
    // 满层（stacks 达 maxStacks）解锁过量治疗；判定用 >=maxStacks 而非硬写 ==5，调参自动跟随。
    if (GetStacks(PerkType::PLANT_REGEN) >= GetInfo(PerkType::PLANT_REGEN).maxStacks)
        return maxHealth * kPlantRegenOverhealMult;
    return maxHealth;
}
