#pragma once

#include "Plant.h"

class Zombie;

/**
 * @brief 只能直接种在水中的一次性植物：锁定近身僵尸后将双方拖入水底。
 *
 * 抓取流程按 C# 原版的 99/51/21/0cs 倒计时推进；anim_grab 仅是附着在僵尸上的视觉，
 * 不依赖新增动画帧事件。
 */
class TangleKelp : public Plant {
public:
	using Plant::Plant;

	void PlantUpdate() override;
	bool CanBeEaten() const override { return false; }
	void Die() override;
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;

	const char* GetTangleKelpStateName() const;
	int GetTargetZombieID() const { return mTargetZombieID; }
	int GetGrabTimeRemainingMs() const;

protected:
	/** 移除通用陆地阴影；水草的根部本来就在水面以下。 */
	void SetupPlant() override;

private:
	enum class State {
		IDLE = 0,
		GRABBING,
	};

	State mState = State::IDLE;
	float mGrabTimer = 0.0f;
	int mTargetZombieID = NULL_ZOMBIE_ID;
	bool mDeathHandled = false;

	/** 选择同一泳池行内与 80x80 近身矩形相交、且最靠近房屋的合法目标。 */
	Zombie* FindTargetZombie() const;
	/** 建立一对一锁定、播放入水反馈并开始 99cs 的抓取倒计时。 */
	void StartGrab(Zombie* target);
	/** 在倒计时跨过原版节点时依次触发拖沉、末段水花和同归于尽。 */
	void UpdateGrab();
	/** 在目标位置生成现有的泳池水花配方。 */
	void EmitSplashAt(const Vector& position) const;
};
