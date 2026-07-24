#pragma once
#ifndef _POLEVAULTER_H
#define _POLEVAULTER_H

#include "Zombie.h"

class Polevaulter : public Zombie {
public:
	using Zombie::Zombie;

	enum class VaultState {
		RUNNING,	// 手持撑杆跑步
		JUMPING,	// 跳跃中
		WALKING		// 跳跃后走路
	};

	VaultState mVaultState = VaultState::RUNNING;
	bool mHasVaulted = false;

	void StartEat(ColliderComponent* other) override;

	/** @brief 开始撑杆跳跃并暂时关闭碰撞与阴影。 */
	void StartJump();
	/** @brief 按品种跳距完成落地，恢复走路状态并调用落地扩展钩子。 */
	void EndJump();
	void JumpMove(float distance);  // 跳跃期间手动移动，传入本次移动距离
	float GetLastVaultDistance() const { return mLastVaultDistance; }

	void HeadDrop() override;
	void ArmDrop() override;
	void ZombieItemUpdate() const override;

	// 撑杆跑/跳阶段不可魅惑（原版：跳过魅惑菇根本不吃它）；落地 WALKING 后可
	bool CanBeCharmed() const override { return mVaultState == VaultState::WALKING; }

	// 跳跃中不可定身（原版 CanBeFrozen 排除 PolevaulterInVault）；减速尾巴照吃（CanBeChilled 不拦）
	bool CanBeFrozen() const override { return mVaultState != VaultState::JUMPING; }

protected:
	void SaveExtraData(nlohmann::json& j) const override;
	void LoadExtraData(const nlohmann::json& j) override;
	void SetupZombie() override;
	void ZombieMove(float scaledDelta, TransformComponent* transform) override;

	// Polevaulter.reanim 无 anim_walk2，走路统一 anim_walk（落地/读档/回走路全经此）。
	void PlayWalkAnimation(float blendTime) override;

	/** @brief 返回本品种一次落地应推进的逻辑距离，单位为像素。 */
	virtual float GetVaultDistance() const;
	/** @brief 落地位移与基础状态恢复完成后的派生行为入口。 */
	virtual void OnVaultLanded() {}

private:
	float mLastVaultDistance = 0.0f;  // 最近一次实际落地位移；仅供稳定测试取证
};

#endif
