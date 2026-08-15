#include "Game/AI/PlantDefenseMonteCarlo.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
	using namespace PlantDefenseMonteCarlo;

	void Require(bool condition, const std::string& message)
	{
		if (!condition) throw std::runtime_error(message);
	}

	Snapshot MakeTreatmentSnapshot()
	{
		Snapshot snapshot;
		snapshot.rows = 5;
		snapshot.columns = 9;
		return snapshot;
	}

	ZombieSnapshot MakeZombie(int id, float x, float y,
		float bodyHealth, float bodyMaxHealth)
	{
		ZombieSnapshot zombie;
		zombie.id = id;
		zombie.row = 2;
		zombie.x = x;
		zombie.y = y;
		zombie.moveSpeed = 20.0f;
		zombie.bodyHealth = bodyHealth;
		zombie.bodyMaxHealth = bodyMaxHealth;
		zombie.attackDamage = 50.0f;
		return zombie;
	}

	TreatmentConfig MakeTreatmentConfig(int sourceZombieId)
	{
		TreatmentConfig config;
		config.combat.rolloutCount = 8;
		config.combat.maxZombiesPerRollout = 16;
		config.combat.horizonSeconds = 3.0f;
		config.combat.stepSeconds = 0.25f;
		config.combat.plantDecisionInterval = 2.0f;
		config.sourceZombieId = sourceZombieId;
		config.castSeconds = 1.0f;
		config.areaRadius = 140.0f;
		config.focusedRadius = 280.0f;
		config.areaHealAmount = 300.0f;
		config.focusedHealAmount = 300.0f;
		config.terminalZombiePressurePerHealth = 0.08f;
		return config;
	}

	NightRoofChargeConfig MakeNightRoofRouteConfig()
	{
		NightRoofChargeConfig config;
		config.combat.rolloutCount = 1;
		config.combat.maxZombiesPerRollout = 16;
		config.combat.horizonSeconds = 3.0f;
		config.combat.stepSeconds = 0.25f;
		config.combat.plantDecisionInterval = 10.0f;
		config.guideImmunitySeconds = 10.0f;
		return config;
	}

	PlantSnapshot MakeRoutePlant(int id, int row, float x,
		float health, float strategicValue)
	{
		PlantSnapshot plant;
		plant.id = id;
		plant.row = row;
		plant.column = 2;
		plant.x = x;
		plant.health = health;
		plant.maxHealth = health;
		plant.strategicValue = strategicValue;
		plant.bounds = { x - 40.0f, 250.0f, 80.0f, 100.0f };
		return plant;
	}

	void TestDelayedCandidateCanWin()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		snapshot.zombies.push_back(MakeZombie(1, 900.0f, 300.0f, 700.0f, 800.0f));
		PlantSnapshot attacker;
		attacker.id = 9;
		attacker.row = 2;
		attacker.column = 2;
		attacker.x = 300.0f;
		attacker.health = 300.0f;
		attacker.maxHealth = 300.0f;
		attacker.strategicValue = 100.0f;
		attacker.attackDps = 100.0f;
		attacker.bounds = { 260.0f, 250.0f, 80.0f, 100.0f };
		snapshot.plants.push_back(attacker);
		const std::vector<TreatmentCandidate> candidates{
			{ TreatmentAction::AREA, -1, 0.0f, 0.0f },
			{ TreatmentAction::AREA, -1, 0.5f, 0.0f },
		};
		const TreatmentResult result = ChooseTreatment(
			snapshot, candidates, {}, MakeTreatmentConfig(1), 0x12345678u);
		Require(result.candidateIndex == 1,
			"moving before the cast should make the delayed treatment branch stronger");
	}

	void TestImmediateCandidateWinsExactDelayTie()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		snapshot.zombies.push_back(MakeZombie(1, 900.0f, 300.0f, 500.0f, 800.0f));
		const std::vector<TreatmentCandidate> candidates{
			{ TreatmentAction::AREA, -1, 0.0f, 0.0f },
			{ TreatmentAction::AREA, -1, 0.5f, 0.0f },
		};
		const TreatmentResult result = ChooseTreatment(
			snapshot, candidates, {}, MakeTreatmentConfig(1), 0x24681357u);
		Require(result.candidateIndex == 0,
			"an exact tie must prefer acting now instead of waiting without benefit");
	}

	void TestHealingHijackerChangesExecutionValue()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		snapshot.zombies.push_back(MakeZombie(1, 900.0f, 300.0f, 800.0f, 800.0f));
		snapshot.zombies.push_back(MakeZombie(2, 850.0f, 300.0f, 100.0f, 400.0f));
		snapshot.zombies.push_back(MakeZombie(3, 880.0f, 300.0f, 100.0f, 800.0f));

		PlantSnapshot plant;
		plant.id = 10;
		plant.row = 2;
		plant.column = 4;
		plant.x = 500.0f;
		plant.health = 300.0f;
		plant.maxHealth = 300.0f;
		plant.strategicValue = 2000.0f;
		plant.bounds = { 460.0f, 250.0f, 80.0f, 100.0f };
		plant.hijackerExecutionGroup = 22;
		plant.countsForHijackerExecution = true;
		plant.diesWithHijackerExecutionGroup = true;
		snapshot.plants.push_back(plant);

		TreatmentConfig config = MakeTreatmentConfig(1);
		config.hijackerZombieId = 3;
		config.hijackerExecutionSeconds = 2.0f;
		const std::vector<TreatmentCandidate> candidates{
			{ TreatmentAction::FOCUSED, 2, 0.0f, 0.0f },
			{ TreatmentAction::FOCUSED, 3, 0.0f, 0.0f },
		};
		const TreatmentResult result = ChooseTreatment(
			snapshot, candidates, {}, config, 0x87654321u);
		Require(result.candidateIndex == 1,
			"healing the locked hijacker should include the larger execution line");
	}

	void TestSixteenZombieHardLimit()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		for (int id = 1; id <= 20; ++id) {
			snapshot.zombies.push_back(MakeZombie(
				id, 900.0f + static_cast<float>(id), 300.0f, 500.0f, 800.0f));
		}
		const std::vector<TreatmentCandidate> candidates{
			{ TreatmentAction::AREA, -1, 0.0f, 0.0f },
		};
		const TreatmentResult result = ChooseTreatment(
			snapshot, candidates, {}, MakeTreatmentConfig(1), 0x13572468u);
		Require(result.sampledZombieCount == 16,
			"treatment rollouts must expose the shared 16-zombie cap");
		Require(Config{}.maxZombiesPerRollout == 16,
			"the shared default rollout cap must remain 16");
	}

	void TestSupportPlantsUseSeparateCapacity()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		for (int id = 1; id <= 140; ++id) {
			PlantSnapshot plant;
			plant.id = id;
			plant.row = id % 5;
			plant.column = id % 9;
			plant.x = 300.0f + static_cast<float>(plant.column) * 80.0f;
			plant.health = 300.0f;
			plant.maxHealth = 300.0f;
			plant.strategicValue = 25.0f;
			plant.bounds = { plant.x - 40.0f, 200.0f, 80.0f, 100.0f };
			snapshot.plants.push_back(plant);
		}
		for (int id = 1001; id <= 1064; ++id) {
			SupportSnapshot support;
			support.id = id;
			support.row = (id - 1001) % 5;
			support.column = (id - 1001) % 9;
			support.x = 300.0f + static_cast<float>(support.column) * 80.0f;
			support.health = 300.0f;
			support.maxHealth = 300.0f;
			support.strategicValue = 25.0f;
			support.bounds = { support.x - 40.0f, 200.0f, 80.0f, 100.0f };
			snapshot.supports.push_back(support);
		}
		snapshot.candidates.push_back({ 0, 0, 300.0f, 250.0f, 1 });

		Config config;
		config.rolloutCount = 1;
		config.horizonSeconds = 0.1f;
		config.stepSeconds = 0.1f;
		const Result result = ChooseTarget(snapshot, config, 0x10203040u);
		Require(result.sampledPlantCount == 128,
			"detailed plants must retain their independent 128-entry cap");
		Require(result.supportPlantCount == 64,
			"ordinary support plants must use the separate per-cell capacity");
	}

	void TestSupportOnlySnapshotStillSupportsRemoval()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		SupportSnapshot support;
		support.id = 7;
		support.row = 2;
		support.column = 4;
		support.x = 500.0f;
		support.health = 300.0f;
		support.maxHealth = 300.0f;
		support.strategicValue = 25.0f;
		support.bounds = { 460.0f, 250.0f, 80.0f, 100.0f };
		snapshot.supports.push_back(support);
		snapshot.candidates.push_back({ 2, 4, 500.0f, 300.0f, 7 });

		Config config;
		config.rolloutCount = 1;
		config.horizonSeconds = 0.1f;
		config.stepSeconds = 0.1f;
		const Result result = ChooseTarget(snapshot, config, 0x50607080u);
		Require(result.candidateIndex == 0 && result.score > 0.0f,
			"a support-only board must still model bungee removal as meaningful");
	}

	void TestNormalPlantBlocksBeforeCompressedSupport()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		PlantSnapshot normal;
		normal.id = 20;
		normal.row = 2;
		normal.column = 4;
		normal.x = 500.0f;
		normal.health = 1000.0f;
		normal.maxHealth = 1000.0f;
		normal.bounds = { 460.0f, 250.0f, 80.0f, 100.0f };
		normal.eatingLayerPriority = 1;
		snapshot.plants.push_back(normal);

		SupportSnapshot support;
		support.id = 10;
		support.row = 2;
		support.column = 4;
		support.x = 500.0f;
		support.health = 100.0f;
		support.maxHealth = 100.0f;
		support.bounds = normal.bounds;
		snapshot.supports.push_back(support);
		snapshot.zombies.push_back(MakeZombie(
			1, 555.0f, 300.0f, 100.0f, 100.0f));
		snapshot.zombies.back().moveSpeed = 0.0f;
		snapshot.zombies.back().attackDamage = 1.0f;
		snapshot.candidates.push_back({ 2, 4, 500.0f, 300.0f, 10 });

		Config config;
		config.rolloutCount = 1;
		config.horizonSeconds = 0.1f;
		config.stepSeconds = 0.1f;
		config.biteInterval = 100.0f;
		config.terminalBlockedSecondUtility = 1.0f;
		config.terminalBlockedSecondsCap = 200000.0f;
		const Result result = ChooseTarget(snapshot, config, 0x90ABCDEFu);
		Require(std::abs(result.coordinationLoss) < 0.001f,
			"removing the under support must not change the current normal-layer blocker");
	}

	void TestGuidedRoutePreservesZombiePressure()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		snapshot.plants.push_back(MakeRoutePlant(10, 2, 300.0f, 100.0f, 1000.0f));
		ZombieSnapshot guide = MakeZombie(1, 355.0f, 300.0f, 150.0f, 270.0f);
		guide.helmHealth = 40.0f;
		guide.helmMaxHealth = 430.0f;
		snapshot.zombies.push_back(guide);

		NightRoofChargeCandidate ordinary;
		ordinary.row = 2;
		ordinary.resolveSeconds = 0.0f;
		ordinary.zombieDamage = 200.0f;
		NightRoofChargeCandidate guided = ordinary;
		guided.guided = true;
		guided.guideZombieId = 1;
		const NightRoofChargeResult result = ChooseNightRoofChargeRoute(
			snapshot, { ordinary, guided }, MakeNightRoofRouteConfig(), 0x31415926u);
		Require(result.candidateIndex == 1,
			"a guided route must value preserving zombie pressure instead of friendly fire");
	}

	void TestOrdinaryRowCanBeatUnhelpfulGuide()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		PlantSnapshot producer = MakeRoutePlant(10, 1, 300.0f, 300.0f, 100.0f);
		producer.sunPerSecond = 100.0f;
		snapshot.plants.push_back(producer);
		ZombieSnapshot guide = MakeZombie(1, 900.0f, 300.0f, 270.0f, 270.0f);
		guide.row = 2;
		guide.helmHealth = 430.0f;
		guide.helmMaxHealth = 430.0f;
		guide.simulatedCombatant = false;
		snapshot.zombies.push_back(guide);

		NightRoofChargeCandidate ordinary;
		ordinary.row = 1;
		ordinary.resolveSeconds = 0.0f;
		NightRoofChargeCandidate guided;
		guided.row = 2;
		guided.resolveSeconds = 0.0f;
		guided.guided = true;
		guided.guideZombieId = 1;
		const NightRoofChargeResult result = ChooseNightRoofChargeRoute(
			snapshot, { ordinary, guided }, MakeNightRoofRouteConfig(), 0x27182818u);
		Require(result.candidateIndex == 0,
			"the planner must keep ordinary rows when their plant shutdown is more valuable");
	}

	void TestGuidedRouteModelsPendingFreezeAndHardControlImmunity()
	{
		Snapshot snapshot = MakeTreatmentSnapshot();
		snapshot.plants.push_back(MakeRoutePlant(10, 2, 300.0f, 100.0f, 1000.0f));
		PlantSnapshot iceSource = MakeRoutePlant(11, 0, 300.0f, 300.0f, 0.0f);
		snapshot.plants.push_back(iceSource);
		ZombieSnapshot guide = MakeZombie(1, 355.0f, 300.0f, 270.0f, 270.0f);
		guide.helmHealth = 430.0f;
		guide.helmMaxHealth = 430.0f;
		snapshot.zombies.push_back(guide);

		NightRoofChargeCandidate ordinary;
		ordinary.row = 2;
		ordinary.resolveSeconds = 0.0f;
		ordinary.zombieDamage = 0.0f;
		ordinary.paralysisSeconds = 0.0f;
		NightRoofChargeCandidate guided = ordinary;
		guided.guided = true;
		guided.guideZombieId = 1;
		NightRoofChargeConfig config = MakeNightRoofRouteConfig();
		config.pendingControlEvents.push_back({
			11, 0.5f, 20.0f, 20.0f, 4.0f, 4.0f
		});
		const NightRoofChargeResult result = ChooseNightRoofChargeRoute(
			snapshot, { ordinary, guided }, config, 0x16180339u);
		Require(result.candidateIndex == 1,
			"pending IceShroom freeze and the guide immunity window must affect route value");
	}
}

int main()
{
	try {
		TestDelayedCandidateCanWin();
		TestImmediateCandidateWinsExactDelayTie();
		TestHealingHijackerChangesExecutionValue();
		TestSixteenZombieHardLimit();
		TestSupportPlantsUseSeparateCapacity();
		TestSupportOnlySnapshotStillSupportsRemoval();
		TestNormalPlantBlocksBeforeCompressedSupport();
		TestGuidedRoutePreservesZombiePressure();
		TestOrdinaryRowCanBeatUnhelpfulGuide();
		TestGuidedRouteModelsPendingFreezeAndHardControlImmunity();
		std::cout << "PlantDefenseMonteCarloTests passed\n";
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "PlantDefenseMonteCarloTests failed: " << error.what() << '\n';
		return 1;
	}
}
