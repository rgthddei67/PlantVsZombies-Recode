#include "PlantDefenseMonteCarlo.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <random>

namespace PlantDefenseMonteCarlo {
namespace {
	constexpr int kMaxSimulationPlants = 128; // 当前植物与短视未来新种植物的固定数组容量
	constexpr int kMaxSnapshotZombies = 64;  // 候选抽样前接受的当前敌方僵尸快照上限
	constexpr int kMaxSimulationZombies = 16; // 单个 rollout 的硬上限，Config 只能在此范围内下调
	constexpr int kMaxSimulationCards = 16;  // 当前卡槽的固定数组容量
	constexpr float kMinimumHealth = 0.001f; // 浮点生命判活阈值
	constexpr float kScoreTieEpsilon = 0.001f; // 候选平均损失分并列容差
	constexpr float kSunUtilityMultiplier = 1.0f; // 未消费阳光折算为玩家效用的倍率
	constexpr float kProducerFutureValueSeconds = 24.0f; // 新产能植物的额外未来价值折算窗口
	constexpr float kMoveSpeedJitter = 0.1f; // rollout 对当前移速施加的正负随机比例
	constexpr float kPlantChoiceJitter = 0.2f; // 玩家选牌/选格启发式的正负随机比例

	struct SimPlant {
		int id = -1;
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

	struct SimZombie {
		int eatingPlantId = -1;
		int row = 0;
		float x = 0.0f;
		float moveSpeed = 0.0f;
		float bodyHealth = 0.0f;
		float helmHealth = 0.0f;
		float shieldHealth = 0.0f;
		float attackDamage = 0.0f;
		float biteCharge = 0.0f;
		bool breached = false;
	};

	struct SimCard {
		const CardSnapshot* profile = nullptr;
		float cooldownRemaining = 0.0f;
	};

	struct SimulationState {
		std::array<SimPlant, kMaxSimulationPlants> plants;
		std::array<SimZombie, kMaxSimulationZombies> zombies;
		std::array<SimCard, kMaxSimulationCards> cards;
		int plantCount = 0;
		int zombieCount = 0;
		int cardCount = 0;
		float sun = 0.0f;
		float breachLoss = 0.0f;
		std::uint64_t reservedCells = 0;
	};

	struct ScenarioUtility {
		float total = 0.0f;
		float coordination = 0.0f;
	};

	float Random01(std::minstd_rand& random)
	{
		return static_cast<float>(random())
			/ static_cast<float>(std::minstd_rand::max());
	}

	float RandomSigned(std::minstd_rand& random)
	{
		return Random01(random) * 2.0f - 1.0f;
	}

	bool CircleOverlapsBounds(
		const Candidate& center, float radius, const Bounds& bounds)
	{
		const float nearestX = std::clamp(
			center.x, bounds.x, bounds.x + bounds.width);
		const float nearestY = std::clamp(
			center.y, bounds.y, bounds.y + bounds.height);
		const float dx = center.x - nearestX;
		const float dy = center.y - nearestY;
		return dx * dx + dy * dy <= radius * radius;
	}

	bool IsAlive(const SimPlant& plant)
	{
		return plant.health > kMinimumHealth;
	}

	bool IsAlive(const SimZombie& zombie)
	{
		return zombie.bodyHealth > kMinimumHealth && !zombie.breached;
	}

	void ApplyZombieDamage(SimZombie& zombie, float damage)
	{
		float remaining = std::max(0.0f, damage);
		const float shieldDamage = std::min(zombie.shieldHealth, remaining);
		zombie.shieldHealth -= shieldDamage;
		remaining -= shieldDamage;
		const float helmDamage = std::min(zombie.helmHealth, remaining);
		zombie.helmHealth -= helmDamage;
		remaining -= helmDamage;
		zombie.bodyHealth = std::max(0.0f, zombie.bodyHealth - remaining);
	}

	float ZombieThreat(const ZombieSnapshot& zombie)
	{
		const float totalHealth = zombie.bodyHealth
			+ zombie.helmHealth + zombie.shieldHealth;
		const float distanceFactor = 1.0f
			+ 400.0f / std::max(100.0f, zombie.x);
		return std::max(1.0f, totalHealth)
			* std::max(1.0f, zombie.attackDamage)
			* std::max(1.0f, zombie.moveSpeed) * distanceFactor;
	}

	// 用带轻微扰动的威胁排序截取当前僵尸，并构建不持有 GameObject 的固定容量状态。
	void BuildInitialState(const Snapshot& snapshot, const Config& config,
		std::uint32_t seed, SimulationState& state)
	{
		std::minstd_rand random(seed);
		state.sun = std::max(0.0f, snapshot.initialSun);

		state.plantCount = std::min(
			static_cast<int>(snapshot.plants.size()), kMaxSimulationPlants);
		for (int i = 0; i < state.plantCount; ++i) {
			const PlantSnapshot& source = snapshot.plants[i];
			state.plants[i] = {
				source.id,
				source.row,
				source.column,
				source.x,
				std::max(0.0f, source.health),
				std::max(1.0f, source.maxHealth),
				std::max(0.0f, source.strategicValue),
				std::max(0.0f, source.attackDps),
				std::max(0, source.attackRowRadius),
				std::max(0.0f, source.sunPerSecond),
				std::max(0.0f, source.productionDelay),
				source.bounds,
				source.pumpkinShell
			};
		}

		state.cardCount = std::min(
			static_cast<int>(snapshot.cards.size()), kMaxSimulationCards);
		for (int i = 0; i < state.cardCount; ++i) {
			state.cards[i].profile = &snapshot.cards[i];
			state.cards[i].cooldownRemaining =
				std::max(0.0f, snapshot.cards[i].cooldownRemaining);
		}

		struct RankedZombie {
			int index = 0;
			float priority = 0.0f;
		};
		std::array<RankedZombie, kMaxSnapshotZombies> ranked{};
		const int snapshotZombieCount = std::min(
			static_cast<int>(snapshot.zombies.size()), kMaxSnapshotZombies);
		for (int i = 0; i < snapshotZombieCount; ++i) {
			ranked[i] = {
				i,
				ZombieThreat(snapshot.zombies[i])
					* (0.75f + Random01(random) * 0.5f)
			};
		}
		std::sort(ranked.begin(), ranked.begin() + snapshotZombieCount,
			[](const RankedZombie& lhs, const RankedZombie& rhs) {
				return lhs.priority > rhs.priority;
			});

		state.zombieCount = std::min({
			snapshotZombieCount,
			std::max(0, config.maxZombiesPerRollout),
			kMaxSimulationZombies
		});
		const float biteInterval = std::max(0.1f, config.biteInterval);
		for (int i = 0; i < state.zombieCount; ++i) {
			const ZombieSnapshot& source =
				snapshot.zombies[ranked[i].index];
			const float speedJitter =
				1.0f + RandomSigned(random) * kMoveSpeedJitter;
			state.zombies[i] = {
				source.eatingPlantId,
				source.row,
				source.x,
				std::max(0.0f, source.moveSpeed * speedJitter),
				std::max(0.0f, source.bodyHealth),
				std::max(0.0f, source.helmHealth),
				std::max(0.0f, source.shieldHealth),
				std::max(0.0f, source.attackDamage),
				source.isEating ? biteInterval : Random01(random) * biteInterval,
				false
			};
		}
	}

	// 与正式范围伤害一致：先找每个命中格的南瓜层，再让外壳一次性承受倍率伤害。
	void ApplyCandidateImpact(
		SimulationState& state, const Candidate& candidate, const Config& config)
	{
		std::array<bool, kMaxSimulationPlants> normalHits{};
		std::array<bool, kMaxSimulationPlants> pumpkinHits{};
		for (int i = 0; i < state.plantCount; ++i) {
			const SimPlant& plant = state.plants[i];
			if (!IsAlive(plant)
				|| !CircleOverlapsBounds(candidate, config.impactRadius, plant.bounds)) {
				continue;
			}

			int pumpkinIndex = -1;
			for (int candidateIndex = 0;
				candidateIndex < state.plantCount; ++candidateIndex) {
				const SimPlant& candidatePlant = state.plants[candidateIndex];
				if (IsAlive(candidatePlant) && candidatePlant.pumpkinShell
					&& candidatePlant.row == plant.row
					&& candidatePlant.column == plant.column) {
					pumpkinIndex = candidateIndex;
					break;
				}
			}
			if (pumpkinIndex >= 0) pumpkinHits[pumpkinIndex] = true;
			else normalHits[i] = true;
		}

		for (int i = 0; i < state.plantCount; ++i) {
			SimPlant& plant = state.plants[i];
			const float damage = pumpkinHits[i]
				? config.impactDamage * config.pumpkinImpactDamageMultiplier
				: (normalHits[i] ? config.impactDamage : 0.0f);
			if (damage > 0.0f) {
				plant.health = std::max(0.0f, plant.health - damage);
			}
		}
	}

	void UpdatePlantProduction(SimulationState& state, float deltaTime)
	{
		for (int i = 0; i < state.plantCount; ++i) {
			SimPlant& plant = state.plants[i];
			if (!IsAlive(plant) || plant.sunPerSecond <= 0.0f) continue;
			if (plant.productionDelay > 0.0f) {
				plant.productionDelay =
					std::max(0.0f, plant.productionDelay - deltaTime);
				continue;
			}
			state.sun += plant.sunPerSecond * deltaTime;
		}
	}

	void UpdateCardCooldowns(SimulationState& state, float deltaTime)
	{
		for (int i = 0; i < state.cardCount; ++i) {
			state.cards[i].cooldownRemaining = std::max(
				0.0f, state.cards[i].cooldownRemaining - deltaTime);
		}
	}

	float RowThreat(const SimulationState& state, int row)
	{
		float threat = 0.0f;
		for (int i = 0; i < state.zombieCount; ++i) {
			const SimZombie& zombie = state.zombies[i];
			if (!IsAlive(zombie) || zombie.row != row) continue;
			const float health = zombie.bodyHealth
				+ zombie.helmHealth + zombie.shieldHealth;
			threat += std::max(1.0f, health)
				* (1.0f + std::max(0.0f, 900.0f - zombie.x) / 900.0f);
		}
		return threat;
	}

	// 模拟高操作玩家从实际卡槽中选取可负担且已冷却的高收益牌，并启发式放到合法格。
	void TryPlantFromCards(const Snapshot& snapshot, SimulationState& state,
		float remainingTime, std::minstd_rand& random)
	{
		int bestCard = -1;
		float bestCardScore = std::numeric_limits<float>::lowest();
		for (int i = 0; i < state.cardCount; ++i) {
			const SimCard& card = state.cards[i];
			if (!card.profile || card.cooldownRemaining > 0.0f
				|| static_cast<float>(card.profile->cost) > state.sun) {
				continue;
			}
			const std::uint64_t available =
				card.profile->legalCellMask & ~state.reservedCells;
			if (available == 0) continue;

			const float futureIncome = card.profile->sunPerSecond
				* std::max(0.0f, remainingTime - card.profile->firstSunDelay);
			const float costDivisor =
				static_cast<float>(std::max(25, card.profile->cost));
			const float baseScore =
				(card.profile->strategicValue + futureIncome) / costDivisor;
			const float score = baseScore
				* (1.0f + RandomSigned(random) * kPlantChoiceJitter);
			if (score > bestCardScore) {
				bestCardScore = score;
				bestCard = i;
			}
		}
		if (bestCard < 0 || state.plantCount >= kMaxSimulationPlants) return;

		const CardSnapshot& card = *state.cards[bestCard].profile;
		const std::uint64_t available = card.legalCellMask & ~state.reservedCells;
		int bestCell = -1;
		float bestCellScore = std::numeric_limits<float>::lowest();
		for (std::size_t cellIndex = 0;
			cellIndex < snapshot.cells.size() && cellIndex < 64; ++cellIndex) {
			if ((available & (1ULL << cellIndex)) == 0) continue;
			const CellSnapshot& cell = snapshot.cells[cellIndex];
			const float threat = RowThreat(state, cell.row);
			float placementBias = threat * 0.002f;
			if (card.sunPerSecond > 0.0f) {
				placementBias += static_cast<float>(
					std::max(0, snapshot.columns - cell.column)) * 3.0f;
			}
			else if (card.attackDps <= 0.0f) {
				placementBias += static_cast<float>(cell.column) * 2.5f;
			}
			else {
				placementBias += static_cast<float>(cell.column) * 1.25f;
			}
			placementBias *=
				1.0f + RandomSigned(random) * kPlantChoiceJitter;
			if (placementBias > bestCellScore) {
				bestCellScore = placementBias;
				bestCell = static_cast<int>(cellIndex);
			}
		}
		if (bestCell < 0) return;

		const CellSnapshot& cell = snapshot.cells[bestCell];
		SimPlant& plant = state.plants[state.plantCount++];
		plant.id = -1;
		plant.row = cell.row;
		plant.column = cell.column;
		plant.x = cell.x;
		plant.health = std::max(1.0f, card.maxHealth);
		plant.maxHealth = plant.health;
		plant.strategicValue = std::max(
			0.0f, card.strategicValue
				+ card.sunPerSecond * kProducerFutureValueSeconds);
		plant.attackDps = std::max(0.0f, card.attackDps);
		plant.attackRowRadius = std::max(0, card.attackRowRadius);
		plant.sunPerSecond = std::max(0.0f, card.sunPerSecond);
		plant.productionDelay = std::max(0.0f, card.firstSunDelay);
		plant.bounds = {
			cell.x - 40.0f, cell.y - 50.0f, 80.0f, 100.0f
		};
		plant.pumpkinShell = card.pumpkinShell;
		// 初始合法性由正式 CanPlantAt 快照决定；这里只阻止同一 rollout 再占用新种格。
		state.reservedCells |= (1ULL << bestCell);
		state.sun -= static_cast<float>(card.cost);
		state.cards[bestCard].cooldownRemaining =
			std::max(0.0f, card.cooldownTime);
	}

	// 将配置画像中的等效 DPS 分派给每个覆盖行最靠近房屋的存活僵尸。
	void UpdatePlantAttacks(SimulationState& state, float deltaTime, int rows)
	{
		for (int plantIndex = 0; plantIndex < state.plantCount; ++plantIndex) {
			const SimPlant& plant = state.plants[plantIndex];
			if (!IsAlive(plant) || plant.attackDps <= 0.0f) continue;
			const int minRow = std::max(0, plant.row - plant.attackRowRadius);
			const int maxRow = std::min(rows - 1, plant.row + plant.attackRowRadius);
			for (int row = minRow; row <= maxRow; ++row) {
				int targetIndex = -1;
				float closestX = std::numeric_limits<float>::max();
				for (int zombieIndex = 0;
					zombieIndex < state.zombieCount; ++zombieIndex) {
					const SimZombie& zombie = state.zombies[zombieIndex];
					if (!IsAlive(zombie) || zombie.row != row
						|| zombie.x + 10.0f < plant.x || zombie.x >= closestX) {
						continue;
					}
					closestX = zombie.x;
					targetIndex = zombieIndex;
				}
				if (targetIndex >= 0) {
					ApplyZombieDamage(
						state.zombies[targetIndex], plant.attackDps * deltaTime);
				}
			}
		}
	}

	// 同 X 叠层时让活动南瓜层胜出；外壳被打破后自然回落到同格内层。
	int FindFrontPlant(const SimulationState& state, const SimZombie& zombie)
	{
		if (zombie.eatingPlantId >= 0) {
			for (int i = 0; i < state.plantCount; ++i) {
				const SimPlant& plant = state.plants[i];
				if (IsAlive(plant) && plant.id == zombie.eatingPlantId) {
					return i;
				}
			}
		}

		int target = -1;
		float frontX = std::numeric_limits<float>::lowest();
		for (int i = 0; i < state.plantCount; ++i) {
			const SimPlant& plant = state.plants[i];
			if (!IsAlive(plant) || plant.row != zombie.row
				|| plant.x > zombie.x + 10.0f || plant.x < frontX) {
				continue;
			}
			if (plant.x == frontX && target >= 0
				&& (!plant.pumpkinShell || state.plants[target].pumpkinShell)) continue;
			frontX = plant.x;
			target = i;
		}
		return target;
	}

	// 以统一速度、接触距离和等效啃咬间隔推进僵尸，不复制任何品种状态机。
	void UpdateZombies(SimulationState& state, const Config& config, float deltaTime)
	{
		const float biteInterval = std::max(0.1f, config.biteInterval);
		for (int i = 0; i < state.zombieCount; ++i) {
			SimZombie& zombie = state.zombies[i];
			if (!IsAlive(zombie)) continue;

			const int plantIndex = FindFrontPlant(state, zombie);
			if (plantIndex < 0) {
				zombie.x -= zombie.moveSpeed * deltaTime;
			}
			else {
				SimPlant& plant = state.plants[plantIndex];
				const float distance = zombie.x - plant.x;
				const bool lockedEatingTarget =
					zombie.eatingPlantId >= 0
					&& zombie.eatingPlantId == plant.id;
				if (!lockedEatingTarget && distance > config.contactDistance) {
					zombie.x = std::max(
						plant.x + config.contactDistance,
						zombie.x - zombie.moveSpeed * deltaTime);
				}
				else {
					zombie.biteCharge += deltaTime;
					while (zombie.biteCharge >= biteInterval
						&& IsAlive(plant)) {
						plant.health = std::max(
							0.0f, plant.health - zombie.attackDamage);
						zombie.biteCharge -= biteInterval;
					}
				}
			}

			if (zombie.x <= config.houseX) {
				zombie.breached = true;
				state.breachLoss += config.breachPenalty;
			}
		}
	}

	// 在时域末端估算正在啃食的僵尸还要被挡多久，使视野外的破墙协同仍能进入评分。
	float TerminalBlockerUtility(
		const SimulationState& state, const Config& config)
	{
		std::array<float, kMaxSimulationPlants> biteDps{};
		const float biteInterval = std::max(0.1f, config.biteInterval);
		for (int zombieIndex = 0;
			zombieIndex < state.zombieCount; ++zombieIndex) {
			const SimZombie& zombie = state.zombies[zombieIndex];
			if (!IsAlive(zombie) || zombie.attackDamage <= 0.0f) continue;
			const int plantIndex = FindFrontPlant(state, zombie);
			if (plantIndex < 0) continue;
			const SimPlant& plant = state.plants[plantIndex];
			const float distance = zombie.x - plant.x;
			const bool lockedEatingTarget =
				zombie.eatingPlantId >= 0
				&& zombie.eatingPlantId == plant.id;
			if (!lockedEatingTarget
				&& (distance > config.contactDistance + 1.0f
					|| distance < -10.0f)) {
				continue;
			}
			biteDps[plantIndex] += zombie.attackDamage / biteInterval;
		}

		float utility = 0.0f;
		for (int plantIndex = 0;
			plantIndex < state.plantCount; ++plantIndex) {
			const SimPlant& plant = state.plants[plantIndex];
			if (!IsAlive(plant) || biteDps[plantIndex] <= 0.0f) continue;
			const float remainingBlockedSeconds = std::min(
				std::max(0.0f, config.terminalBlockedSecondsCap),
				plant.health / biteDps[plantIndex]);
			utility += remainingBlockedSeconds
				* std::max(0.0f, config.terminalBlockedSecondUtility);
		}
		return utility;
	}

	// 把剩余阳光、植物战略价值和越线惩罚归一成候选之间可比较的玩家效用。
	float PlayerUtility(const SimulationState& state)
	{
		float utility = state.sun * kSunUtilityMultiplier - state.breachLoss;
		for (int i = 0; i < state.plantCount; ++i) {
			const SimPlant& plant = state.plants[i];
			if (!IsAlive(plant)) continue;
			const float healthFraction = std::clamp(
				plant.health / std::max(1.0f, plant.maxHealth), 0.0f, 1.0f);
			utility += plant.strategicValue * healthFraction;
		}
		return utility;
	}

	// 执行一个固定时域场景；candidate 为空时即为同随机种子的无攻击基线。
	ScenarioUtility RunScenario(const Snapshot& snapshot, const Config& config,
		const Candidate* candidate, std::uint32_t seed)
	{
		SimulationState state;
		BuildInitialState(snapshot, config, seed, state);
		if (candidate) ApplyCandidateImpact(state, *candidate, config);

		std::minstd_rand random(seed ^ 0x9E3779B9u);
		const float deltaTime = std::max(0.1f, config.stepSeconds);
		const int stepCount = std::max(
			1, static_cast<int>(std::ceil(config.horizonSeconds / deltaTime)));
		float nextPlantDecision = Random01(random)
			* std::max(deltaTime, config.plantDecisionInterval);

		for (int step = 0; step < stepCount; ++step) {
			const float elapsed = static_cast<float>(step) * deltaTime;
			const float remaining = std::max(
				0.0f, config.horizonSeconds - elapsed);
			UpdatePlantProduction(state, deltaTime);
			UpdateCardCooldowns(state, deltaTime);
			if (elapsed >= nextPlantDecision) {
				TryPlantFromCards(snapshot, state, remaining, random);
				nextPlantDecision += std::max(
					deltaTime, config.plantDecisionInterval);
			}
			UpdatePlantAttacks(state, deltaTime, snapshot.rows);
			UpdateZombies(state, config, deltaTime);
		}
		const float coordination = TerminalBlockerUtility(state, config);
		return {
			PlayerUtility(state) + coordination,
			coordination
		};
	}
}

Result ChooseTarget(const Snapshot& snapshot, const Config& config, std::uint32_t seed)
{
	Result result;
	result.rolloutCount = std::max(1, config.rolloutCount);
	result.sampledZombieCount = std::min({
		static_cast<int>(snapshot.zombies.size()),
		std::max(0, config.maxZombiesPerRollout),
		kMaxSimulationZombies
	});
	result.cardCount = std::min(
		static_cast<int>(snapshot.cards.size()), kMaxSimulationCards);
	if (snapshot.candidates.empty() || snapshot.plants.empty()
		|| snapshot.rows <= 0 || snapshot.columns <= 0) {
		return result;
	}

	std::vector<ScenarioUtility> baselineUtilities(result.rolloutCount);
	for (int rollout = 0; rollout < result.rolloutCount; ++rollout) {
		const std::uint32_t rolloutSeed =
			seed + static_cast<std::uint32_t>(rollout) * 0x85EBCA6Bu;
		baselineUtilities[rollout] =
			RunScenario(snapshot, config, nullptr, rolloutSeed);
	}

	float bestScore = std::numeric_limits<float>::lowest();
	for (std::size_t candidateIndex = 0;
		candidateIndex < snapshot.candidates.size(); ++candidateIndex) {
		float totalLoss = 0.0f;
		float totalCoordinationLoss = 0.0f;
		for (int rollout = 0; rollout < result.rolloutCount; ++rollout) {
			const std::uint32_t rolloutSeed =
				seed + static_cast<std::uint32_t>(rollout) * 0x85EBCA6Bu;
			const ScenarioUtility attackedUtility = RunScenario(
				snapshot, config, &snapshot.candidates[candidateIndex], rolloutSeed);
			totalLoss +=
				baselineUtilities[rollout].total - attackedUtility.total;
			totalCoordinationLoss +=
				baselineUtilities[rollout].coordination
				- attackedUtility.coordination;
		}
		const float averageLoss =
			totalLoss / static_cast<float>(result.rolloutCount);
		const float averageCoordinationLoss =
			totalCoordinationLoss / static_cast<float>(result.rolloutCount);
		if (averageLoss > bestScore + kScoreTieEpsilon) {
			bestScore = averageLoss;
			result.candidateIndex = static_cast<int>(candidateIndex);
			result.score = averageLoss;
			result.coordinationLoss = averageCoordinationLoss;
		}
	}
	return result;
}

} // namespace PlantDefenseMonteCarlo
