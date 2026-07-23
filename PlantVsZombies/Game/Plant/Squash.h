#pragma once

#include "Plant.h"

class Zombie;

class Squash : public Plant {
public:
	using Plant::Plant;

	/** 推进 C# 原版的观察、起跳、砸落和落地清理状态机。 */
	void PlantUpdate() override;
	/** 起跳流程开始后忽略啃食伤害，与原版 SquashLook/PreLaunch 无敌窗口一致。 */
	void TakeDamage(int damage, DamageSource source) override;
	/** 保存状态机计时、锁定目标与落点，使起跳途中存档不会重置攻击。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复自定义状态后重建空中位置、碰撞开关和阴影偏移。 */
	void LoadExtraData(const nlohmann::json& j) override;

	const char* GetSquashStateName() const;
	int GetTargetZombieID() const { return mTargetZombieID; }
	bool HasAppliedDamage() const { return mDamageApplied; }

protected:
	/** 设置原版待机速度区间和落地阴影基准。 */
	void SetupPlant() override;

private:
	enum class State {
		IDLE = 0,
		LOOKING,
		PRELAUNCH,
		RISING,
		FALLING,
		LANDED,
	};

	State mState = State::IDLE;
	float mStateTimer = 0.0f;
	int mTargetZombieID = NULL_ZOMBIE_ID;
	float mTargetX = 0.0f;
	bool mDamageApplied = false;

	/** 保留仍有效的旧目标，否则选择本行距离攻击矩形最近且未被其他倭瓜锁定的僵尸。 */
	int FindTargetZombieID() const;
	/** 检查原版头部、碰撞与特殊阶段条件，并输出用于择近的水平间距分数。 */
	bool IsValidTarget(Zombie* zombie, float& score) const;
	/** 防止多个倭瓜同时锁定同一个僵尸。 */
	bool IsTargetedByOtherSquash(int zombieID) const;
	/** 锁定目标并播放朝向观察动画与随机 Hmm 音效。 */
	void StartLooking(Zombie* target);
	/** 进入 0.3 秒跳跃预备阶段。 */
	void StartPreLaunch();
	/** 起跳前重新索敌并按目标速度预测最终落点。 */
	void StartRising();
	/** 从最高点进入 0.1 秒快速下砸阶段。 */
	void StartFalling();
	/** 区分草地压扁停留与水路溅水后立即消失。 */
	void FinishFalling();
	/** 对落点攻击矩形结算原版 1800 阈值直杀或重型目标伤害。 */
	void ApplySquashDamage();
	/** 读档后按状态和剩余计时恢复逻辑位置与碰撞状态。 */
	void RestoreStatePosition();
	/** 前 0.3 秒平滑移动到落点上方，后 0.2 秒悬停。 */
	void UpdateRisingPosition();
	/** 将最高点线性插值到实际落点。 */
	void UpdateFallingPosition();
	/** 让阴影固定在原种植格地面，植物本体独立跳跃。 */
	void UpdateShadowOffset();
	/** 返回目标所在列的地面落点 Y。 */
	float GetLandingY() const;
	/** 把预测落点 X 转换并钳制为有效棋盘列。 */
	int GetLandingColumn() const;
};
