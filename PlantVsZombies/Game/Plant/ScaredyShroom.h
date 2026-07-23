#pragma once
#ifndef _H_SCAREDYSHROOM_H
#define _H_SCAREDYSHROOM_H

#include "Shroom.h"
#include "../Board.h"

// 胆小菇：夜间廉价射手。僵尸靠近（圆半径120，覆盖本行±1行、含身后）时
// 吓得缩进土里停火，走开后重新升起。行为镜像原版 Plant::UpdateScaredyShroom。
class ScaredyShroom : public Shroom
{
public:
	using Shroom::Shroom;

	// 害怕状态机：READY→LOWERING(anim_scared)→SCARED(anim_scaredidle循环)→RAISING(anim_grow)→READY
	enum class FearState { READY = 0, LOWERING, SCARED, RAISING };

	void SaveExtraData(nlohmann::json& j) const override {
		j["shootTimer"] = mShootTimer;
		j["fearState"] = static_cast<int>(mFearState);
	}

	void LoadExtraData(const nlohmann::json& j) override {
		mShootTimer = j.value("shootTimer", 1.0f);
		int state = j.value("fearState", 0);
		if (state < 0 || state > static_cast<int>(FearState::RAISING)) state = 0;
		mFearState = static_cast<FearState>(state);
		mShotPending = GetCurrentTrackName() == "anim_shooting";
	}

	void PlantUpdate() override;

protected:
	FearState mFearState = FearState::READY;
	float mCheckZombieTimer = 0.0f;
	float mShootTime = 1.5f;     // 射击间隔时间
	float mShootTimer = 1.0f;    // 射击计时器
	// 初始即到期：首帧必须真算，不能吃 mScaredCached 的初始 false——
	// 否则读档恢复 SCARED 态的第一帧会误判"僵尸走了"，先伸头再缩回去
	float mFearCheckTimer = 1.0f;
	bool mScaredCached = false;  // 害怕判定节流缓存（0.1s 重算一次）
	bool mTargetCached = false;  // 本行索敌节流缓存（0.1s 重算一次，支持最快 0.2s 连射）
	bool mShotPending = false;   // 射击动画已起播但第 25 帧孢子事件尚未结算

	bool HasZombieInRow();       // 索敌：本行前方全行射程（不像小喷菇限 300px）
	bool HasZombieNearby();      // 害怕判定：圆半径 120 与僵尸碰撞矩形相交，查本行±1行

	/** 返回本轮发射所用的射击间隔；精英变体按成长阶段覆写。 */
	virtual float GetShootInterval() const { return mShootTime; }
	/** 返回当前射击动画速度，使吐弹帧事件始终早于下一轮射击。 */
	virtual float GetShootAnimationSpeed(float attackSpeedMultiplier) const;
	/** 返回本轮孢子基础伤害；生存词条仍由僵尸受击入口统一缩放。 */
	virtual int GetPuffDamage() const { return 20; }
	/** 一发孢子成功创建后的扩展入口。 */
	virtual void OnPuffFired() {}
	/** 从 READY 首次进入害怕状态时的扩展入口。 */
	virtual void OnFearStarted() {}

	void SetupPlant() override;
};

#endif
