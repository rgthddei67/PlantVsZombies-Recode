#pragma once
#ifndef _ZOMBIE_H
#define _ZOMBIE_H

#include "ZombieType.h"
#include "../AnimatedObject.h"
#include "../Plant/PlantType.h"
#include "../../DeltaTime.h"
#include "../../GameRandom.h"
#include "../EntityManager.h"
#include <nlohmann/json.hpp>

class Board;
class Plant;

class Zombie : public AnimatedObject {
public:
	Board* mBoard = nullptr;
	ZombieType mZombieType = ZombieType::NUM_ZOMBIE_TYPES;

	int mRow = -1;

	int mAttackDamage = 50;
	int mFreeHitsRemaining = 0;	// 词条②：剩余免伤次数（出生时=10×层数，0=无效）

	bool mNeedDropArm = true;
	bool mNeedDropHead = true;
	int mZombieID = NULL_ZOMBIE_ID;

	int mSpawnWave = -1;	// 多少波刷新的

	int mBodyHealth = 270;
	int mBodyMaxHealth = 270;
	HelmType mHelmType = HelmType::HELMTYPE_NONE;
	int mHelmHealth = 0;
	int mHelmMaxHealth = 0;
	ShieldType mShieldType = ShieldType::SHIELDTYPE_NONE;
	int mShieldHealth = 0;
	int mShieldMaxHealth = 0;

protected:
	Vector mVisualOffset;   // 视觉偏移量
	bool mIsPreview = false;

	float mExtraSpeed = 1.0f;

	float mCooldownTimer = 0.0f;	// 僵尸减速倒计时时间

	bool mIsMindControlled = false;	//有没有被魅惑

	bool mIsEating = false;
	int mEatPlantID = NULL_PLANT_ID;
	int mEatZombieID = NULL_ZOMBIE_ID;   // 互啃目标（魅惑↔普通）；不持久化——读档后由碰撞下一帧重建

	bool mHasHead = true;
	bool mHasArm = true;
	bool mHasTongue = false;
	bool mIsDying = false;	// 是否播放死亡动画 大概可以这么理解 这个时候不能走路
	bool mIsDead = false;	// Die() 防重入：外部（大嘴花/土豆雷/清场）与帧事件可能同帧重复调 Die，
							// 第二次进入会把 mZombieNumber 多扣一次导致计数提前归零
	bool mDbgAnomalyLogged = false;	// [DBG] 临时插桩：死亡期间轨道异常只记一次

	float mSpeed = 10.0f;
	int mGroundTrackIndex = -1;

private:
	float mCheckPositionTimer = 0.0f;
	float mSubHealthTimer = 0.0f;	
	float mDyingTimer = 0.0f;	// mIsDying 持续时间，超过 10s 强制 Die 防止卡 BUG

public:
	Zombie(Board* board, ZombieType zombieType, float x, float y, int row,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);
	~Zombie() = default;

	void Start() override;
	void Update() override;
	void Draw(Graphics* g) override;	// 重写以叠加血量显示
	virtual void ZombieUpdate(float scaledTime) {}		// 子类重写Update用这个
	// penetrateShield=true：穿透二类护盾（大喷菇喷雾）——护盾照常受损/掉落，
	// 但全额伤害继续透到头盔+本体（还原原版 DoRowAreaDamage(20, 2U) 的位标志语义）。
	virtual void TakeDamage(int damage, bool penetrateShield = false);
	virtual void SaveExtraData(nlohmann::json& j) const {}	// 保存额外数据
	virtual void LoadExtraData(const nlohmann::json& j) {}	// 加载额外数据
	virtual void ZombieItemUpdate() const; // 处理僵尸读档的时候的手臂、防具等处理
	virtual void Charred();	// 变成灰烬

	virtual int TakeShieldDamage(int damage);
	virtual int TakeHelmDamage(int damage);
	virtual void TakeBodyDamage(int damage);

	int GetSortingKey() const override { return this->mRow; }

	void CheckWin() const;		// 生成奖杯判断

	virtual void ShieldDrop();		// 二类防具掉落 必须调用Zombie::SheildDrop
	virtual void HelmDrop();	// 一类防具掉落 必须调用Zombie::HelmDrop
	virtual void HeadDrop();	// 头掉落 不用调用Zombie::HeadDrop
	virtual void ArmDrop();		// 手掉落 不用调用Zombie::ArmDrop

	virtual void Die();
	virtual void StartEat(ColliderComponent* other);
	virtual void StopEat(ColliderComponent* other);
	virtual void EatTarget();	// 吃东西掉血的函数

	Vector GetVisualPosition() const override;
	Vector GetPosition() const;
	void SetPosition(const Vector& position);

	bool IsMindControlled() const { return this->mIsMindControlled; }
	// 魅惑唯一入口：豁免(CanBeCharmed)/重复/垂死则 no-op。魅惑菇、AutoTest charm_zombie 都走这里。
	void StartMindControlled();
	// 子类豁免点：不可魅惑态（如撑杆 RUNNING/JUMPING）返回 false
	virtual bool CanBeCharmed() const { return true; }
	bool HasHead() const { return this->mHasHead; }
	bool IsDying() const { return this->mIsDying; }
	bool HasArm() const { return this->mHasArm; }
	float GetCooldownTimer() const { return this->mCooldownTimer; }

	virtual void SetCooldown(float timer);		// 设置僵尸减速状态

	void SaveProtectedData(nlohmann::json& j) const;

	void LoadProtectedData(const nlohmann::json& j);

	void ValidateEatingState(EntityManager& em);

	// 将本体/头盔/护盾的当前血量与上限整体按倍率缩放（与具体模式无关，由调用方决定倍率来源）。
	// 倍率<=0 或 ==1 时不作处理；缩放后保持 current==max（同源同舍入）。
	void ApplyHealthMultiplier(double multiplier);

protected:
	virtual void ZombieMove(float scaledDelta, TransformComponent* transform);

	// 这才是设置僵尸
	virtual void SetupZombie();

	virtual void CheckHelmImage() {}	// 检查是否应该更换一类防具图片
	virtual void CheckShieldImage() {} 	// 检查是否应该更换二类防具图片

	// ═══ 啃食 → 走路 状态机：扩展新僵尸只需覆写下面带 ★ 的 virtual，永不碰 ResumeWalkAfterEat ═══

	// ★关注点A｜"我此刻怎么走路"的唯一权威：播哪条稳态走路轨道 + clip 速度（须确定性，勿随机）。
	//   啃完回走路 / 撑杆落地 / 读档恢复 全经它，所以"改走路动画"永远只改这一处。
	//   何时覆写：reanim 没有 anim_walk2，或走路轨道随状态变。
	//   例（读报僵尸·狂暴撕报后换轨道并带 clip 速度）：
	//     void PlayWalkAnimation(float blend) override {
	//         if (mHasNewspaper) PlayTrack("anim_walk", 0.0f, blend);
	//         else               PlayTrack("anim_walk_nopaper", kNoPaperWalkClip, blend);
	//     }
	virtual void PlayWalkAnimation(float blendTime = 0.0f);

	// ★关注点B｜啃食视觉残留（一对对称钩子，默认空操作，绝大多数僵尸不用管）：
	//   OnStartEating 一开吃触发、OnStopEating 一停吃触发。在 StartEat 里改过的视觉状态，在这对里对称还原。
	//   例（铁门僵尸·啃食露常规手臂、收尾藏回门后，门还在才动）：
	//     void OnStartEating() override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(true);  }
	//     void OnStopEating()  override { if (mShieldType != ShieldType::SHIELDTYPE_NONE) ShowArm(false); }
	virtual void OnStartEating() {}
	virtual void OnStopEating()  {}

	// 模板方法（非虚，勿覆写）：啃完回走路 = 先收尾、再走路。执行顺序由基类锁死。
	void ResumeWalkAfterEat(float blendTime) { OnStopEating(); PlayWalkAnimation(blendTime); }
	// 魅惑的派生状态（碰撞掩码+视觉）：StartMindControlled 与读档恢复共用
	void ApplyCharmEffects();

private:
	void WinGame() const;	// 植物胜利
};

#endif