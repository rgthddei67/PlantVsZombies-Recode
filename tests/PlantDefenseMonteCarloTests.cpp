#include "Game/AI/PlantDefenseMonteCarlo.h"

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
}

int main()
{
	try {
		TestDelayedCandidateCanWin();
		TestImmediateCandidateWinsExactDelayTie();
		TestHealingHijackerChangesExecutionValue();
		TestSixteenZombieHardLimit();
		std::cout << "PlantDefenseMonteCarloTests passed\n";
		return 0;
	}
	catch (const std::exception& error) {
		std::cerr << "PlantDefenseMonteCarloTests failed: " << error.what() << '\n';
		return 1;
	}
}
