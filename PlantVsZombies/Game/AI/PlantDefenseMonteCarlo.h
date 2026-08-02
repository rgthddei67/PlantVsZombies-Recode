#pragma once

#include <cstdint>
#include <vector>

namespace PlantDefenseMonteCarlo {

struct Bounds {
	float x = 0.0f;
	float y = 0.0f;
	float width = 0.0f;
	float height = 0.0f;
};

struct PlantSnapshot {
	int id = 0;
	int row = 0;
	int column = 0;
	float x = 0.0f;
	float health = 0.0f;
	float maxHealth = 1.0f;
	float strategicValue = 0.0f;
	float attackDps = 0.0f;
	int attackRowRadius = 0;
	float sunPerSecond = 0.0f;
	float productionDelay = 0.0f;
	Bounds bounds;
	bool pumpkinShell = false;
};

struct ZombieSnapshot {
	int id = 0;
	int eatingPlantId = -1;
	int row = 0;
	float x = 0.0f;
	float moveSpeed = 0.0f;
	float bodyHealth = 0.0f;
	float helmHealth = 0.0f;
	float shieldHealth = 0.0f;
	float attackDamage = 0.0f;
	bool isEating = false;
};

struct CardSnapshot {
	int typeKey = 0;
	int cost = 0;
	float cooldownRemaining = 0.0f;
	float cooldownTime = 0.0f;
	float maxHealth = 300.0f;
	float strategicValue = 0.0f;
	float attackDps = 0.0f;
	int attackRowRadius = 0;
	float sunPerSecond = 0.0f;
	float firstSunDelay = 0.0f;
	std::uint64_t legalCellMask = 0;
	bool pumpkinShell = false;
};

struct CellSnapshot {
	int row = 0;
	int column = 0;
	float x = 0.0f;
	float y = 0.0f;
	bool occupied = false;
};

struct Candidate {
	int row = 0;
	int column = 0;
	float x = 0.0f;
	float y = 0.0f;
};

struct Snapshot {
	int rows = 0;
	int columns = 0;
	float initialSun = 0.0f;
	std::vector<PlantSnapshot> plants;
	std::vector<ZombieSnapshot> zombies;
	std::vector<CardSnapshot> cards;
	std::vector<CellSnapshot> cells;
	std::vector<Candidate> candidates;
};

struct Config {
	int rolloutCount = 32;                  // 每个候选使用的短视未来样本数
	int maxZombiesPerRollout = 12;          // 单次样本最多推进的当前敌方僵尸数
	float horizonSeconds = 16.0f;           // 单次样本向前推演的游戏秒
	float stepSeconds = 0.25f;              // 固定数值步长，越小越精细但开销越高
	float impactDamage = 50.0f;             // 候选动作在 t=0 对植物造成的伤害
	float impactRadius = 100.0f;             // 候选动作的圆形爆区半径，单位 px
	float pumpkinImpactDamageMultiplier = 3.0f; // 南瓜头拦截候选范围伤害时的输入倍率
	float biteInterval = 1.0f;               // 简化啃咬的等效间隔，单位游戏秒
	float contactDistance = 55.0f;           // 僵尸进入啃咬态的水平距离，单位 px
	float plantDecisionInterval = 2.0f;      // 玩家在样本中尝试种下一株植物的间隔秒数
	float houseX = 110.0f;                   // 僵尸越过此坐标后的防线失守判定
	float breachPenalty = 1500.0f;           // 每只越线僵尸从玩家效用中扣除的分数
	float terminalBlockedSecondUtility = 12.0f; // 终局每秒剩余破墙时间折算的玩家防守效用
	float terminalBlockedSecondsCap = 90.0f; // 单个阻挡植物计入终局分的最大剩余破墙秒数
};

struct Result {
	int candidateIndex = -1;
	float score = 0.0f;
	int rolloutCount = 0;
	int sampledZombieCount = 0;
	int cardCount = 0;
	float coordinationLoss = 0.0f;
};

/**
 * @brief 用当前实体和卡槽的轻量快照比较候选攻击，返回令玩家未来效用损失最大的落点。
 *
 * 每个候选与无攻击基线使用相同 rollout seed；函数只使用局部随机数，不推进游戏全局 RNG。
 */
Result ChooseTarget(const Snapshot& snapshot, const Config& config, std::uint32_t seed);

} // namespace PlantDefenseMonteCarlo
