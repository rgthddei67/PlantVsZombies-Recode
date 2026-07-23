#pragma once
#ifndef _ZOMBIE_H
#define _ZOMBIE_H

#include "ZombieType.h"
#include "../AnimatedObject.h"
#include "../Plant/PlantType.h"
#include "../../DeltaTime.h"
#include "../../GameRandom.h"
#include "../EntityManager.h"
#include "../DamageSource.h"
#include <nlohmann/json.hpp>

class Board;
class Plant;

class Zombie : public AnimatedObject {
public:
	Board* mBoard = nullptr;
	ZombieType mZombieType = ZombieType::NUM_ZOMBIE_TYPES;

	int mRow = -1;

	int mAttackDamage = 50;
	int mFreeHitsRemaining = 0;	// 词条：剩余免伤次数（出生时=4×层数，0=无效）

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
	float mFrozenTimer = 0.0f;		// 冻结剩余秒数（寒冰菇完全定身），0=未冻结

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
	// source 必填，使植物增伤只作用于植物来源。penetrateShield=true：穿透二类护盾（大喷菇喷雾）——护盾照常受损/掉落，
	// 但全额伤害继续透到头盔+本体（还原原版 DoRowAreaDamage(20, 2U) 的位标志语义）。
	// discardShieldOverflow=true：若命中开始时存在二类护盾，则本击止于护盾，破盾溢出也不进入头盔/本体。
	virtual void TakeDamage(int damage, DamageSource source, bool penetrateShield = false,
		bool discardShieldOverflow = false);
	/** 植物爆炸的统一入口：默认按原版阈值化灰，否则走带 PLANT_ASH 分类的普通扣血链。 */
	virtual void TakePlantAshDamage(int damage);
	/** 大嘴花等植物直杀的统一入口；返回是否确实吞掉目标，以决定是否进入消化状态。 */
	virtual bool TakePlantInstantKill();
	virtual void SaveExtraData(nlohmann::json& j) const {}	// 保存额外数据
	virtual void LoadExtraData(const nlohmann::json& j) {}	// 加载额外数据
	virtual void ZombieItemUpdate() const; // 处理僵尸读档的时候的手臂、防具等处理
	virtual void Charred();	// 变成灰烬
	/** 是否允许灰烬攻击进入化灰表现；特殊僵尸可覆写为 false 并承受数值伤害。 */
	virtual bool CanBeCharred() const { return true; }
	/** 是否在当前状态截断大喷菇区域攻击；调用方必须在本次伤害结算前取值。 */
	virtual bool BlocksFumePiercing() const { return false; }
	/** 调整大喷菇对本体的基础伤害；返回值随后统一进入词条与防具结算。 */
	virtual int ModifyFumeDamage(int damage) const { return damage; }

	virtual int TakeShieldDamage(int damage);
	virtual int TakeHelmDamage(int damage);
	virtual void TakeBodyDamage(int damage);
	/** 词条缩放后的最终伤害修正点；用于按来源和当前防具状态实施每击上限。 */
	virtual int AdjustIncomingDamage(int damage, DamageSource source, bool penetrateShield) const {
		return damage;
	}

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
	/** 与小推车接触时是否静默吞掉其他行小推车；当前行仍维持原版碰撞结算。 */
	virtual bool ConsumesOtherMowersOnContact() const { return false; }

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
	bool IsEating() const { return this->mIsEating; }
	bool HasArm() const { return this->mHasArm; }
	float GetCooldownTimer() const { return this->mCooldownTimer; }
	bool IsFrozen() const { return this->mFrozenTimer > 0.0f; }
	float GetFrozenTimer() const { return this->mFrozenTimer; }

	// 冻结唯一入口（寒冰菇）：先上 20s 减速尾巴（SetCooldown，其持盾守卫保留——持盾照冻不吃减速），
	// 再完全定身（首冻 4~6s / 已减速或已冻再冻 3~4s，原版 HitIceTrap 语义）。伤害由调用方另行结算。
	// 返回 true=进入冻结；豁免（魅惑/濒死/预览/CanBeFrozen 覆写）返回 false。
	bool StartFrozen();
	// 行为守卫放虚函数（skill 教训：勿放 lambda）。Chilled=减速+冻结的总闸（魅惑免疫在基类）；
	// Frozen=仅豁免定身、减速尾巴照上（如撑杆跳跃中，原版 CanBeFrozen 语义）。
	virtual bool CanBeChilled() const;
	virtual bool CanBeFrozen() const { return true; }

	virtual void SetCooldown(float timer);		// 设置僵尸减速状态
	// Board 天气切换时调用，统一重算 extra 层；不会覆盖 PlayTrack 的 clip 速度。
	void RefreshAnimSpeedForWeather() { UpdateAnimSpeed(); }

	void SaveProtectedData(nlohmann::json& j) const;

	void LoadProtectedData(const nlohmann::json& j);

	void ValidateEatingState(EntityManager& em);

	// 将本体/头盔/护盾的当前血量与上限整体按倍率缩放（与具体模式无关，由调用方决定倍率来源）。
	// 倍率<=0 或 ==1 时不作处理；缩放后保持 current==max（同源同舍入）。
	void ApplyHealthMultiplier(double multiplier);

protected:
	// 统一重算动画 extra 速度层：冻结(0) > 减速(mExtraSpeed×因子) > 常速(mExtraSpeed)。
	// 所有改 mExtraSpeed/减速/冻结状态的运行期路径都必须经此收敛（原版 UpdateAnimSpeed 等价物），
	// 直调 SetExtraSpeedMultiplier 会把冻结停格顶掉（出生 Setup 除外——彼时不可能已冻结）。
	void UpdateAnimSpeed();
	// 减速时动画降速因子（快速铁桶 0.8 覆写；位移减半由 Update 的 scaledDelta 承担，与此正交）
	virtual float GetSlowAnimFactor() const { return 0.6f; }
	// 品种能力层动画倍率；天气等外部状态变化后由 Board 统一触发重算。
	virtual float GetAbilityAnimSpeedMultiplier() const { return 1.0f; }
	// 解除冻结并恢复动画速度；蓝色 overlay 仅在无减速尾巴时清除（持盾僵尸没有尾巴→立即褪色）
	void ClearFrozen();

	/** 叠加活动阵风的物理漂移；不依赖自主行走、啃食、冻结或魅惑方向。 */
	void ApplyTyphoonGustDrift(float deltaTime, TransformComponent* transform);
	virtual void ZombieMove(float scaledDelta, TransformComponent* transform);

	// 这才是设置僵尸
	virtual void SetupZombie();
	/** 注册当前 reanim 时间轴上的死亡与啃食帧事件；帧布局不同的品种在此替换。 */
	virtual void RegisterFrameEvents();
	/** 返回掉头流血结束后应播放的死亡轨道名。 */
	virtual const char* GetDeathTrackName() const { return "anim_death"; }

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
	/** 双层水面种植下只允许选中格子的最上层植物，并尊重植物自己的短期啃食保护。 */
	bool IsPlantValidEatTarget(Plant* plant) const;
	/** 把同格啃食目标迁移到新出现的上层植物，并保持双方 mEaterCount 平衡。 */
	bool RetargetPlantIfHigherPriority(Plant* plant);

	// 模板方法（非虚，勿覆写）：啃完回走路 = 先收尾、再走路。执行顺序由基类锁死。
	void ResumeWalkAfterEat(float blendTime) { OnStopEating(); PlayWalkAnimation(blendTime); }
	// 魅惑的派生状态（碰撞掩码+视觉）：StartMindControlled 与读档恢复共用
	void ApplyCharmEffects();

private:
	void WinGame() const;	// 植物胜利
};

#endif
