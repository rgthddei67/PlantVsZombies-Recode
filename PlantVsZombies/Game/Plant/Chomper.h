#pragma once
#ifndef _CHOMPER_H
#define _CHOMPER_H
#include "Plant.h"
#include "../Zombie/ZombieType.h"

class Chomper : public Plant {
private:
	enum class State { IDLE = 0, BITING = 1, DIGESTING = 2 };

	State mState = State::IDLE;
	float mDigestTimer = 0.0f;        // 0..30 during DIGESTING
	float mDetectionTimer = 0.0f;     // 节流扫描计时器
	int   mTargetZombieID = NULL_ZOMBIE_ID;

	static constexpr float CHOMP_RANGE_X = 1.7f * 95.0f; // 1.5 * CELL_COLLIDER_SIZE_X
	static constexpr float DIGEST_DURATION = 30.0f;        // 吞噬后冷却 30s
	static constexpr float DETECTION_INTERVAL = 0.2f;
	static constexpr int   BITE_KILL_FRAME = 43;           // anim_bite 中触发吞食或拒吞咬伤结算的帧
	static constexpr int   REJECTED_BITE_DAMAGE = 20;      // 目标拒绝被吞食时结算的基础植物伤害

	int  FindTargetZombieID();
	void StartBite(int zombieID);
	void OnBiteKillFrame();
	void EndDigest();

public:
	using Plant::Plant;

	void PlantUpdate() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
};

#endif
