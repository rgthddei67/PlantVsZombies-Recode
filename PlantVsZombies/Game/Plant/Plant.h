#pragma once
#ifndef _PLANT_H
#define _PLANT_H
#include <iostream>
#include <algorithm>
#include <memory>
#include <nlohmann/json.hpp>
#include "./PlantType.h"
#include "../ColliderComponent.h"
#include "../TransformComponent.h"
#include "../AnimatedObject.h"
#include "../AudioSystem.h"
#include "../../GameRandom.h"
#include "../../DeltaTime.h"
#include "../EntityManager.h"
#include "../DamageSource.h"
#include "../Zombie/ZombieJumpType.h"
#include "../Zombie/ZombieType.h"

class Board;
class Zombie;

enum class PlantBungeeState {
	NONE,
	GRABBING,
	RISING,
};

class Plant : public AnimatedObject {
public:
	Board* mBoard = nullptr;
	PlantType mPlantType = PlantType::NUM_PLANT_TYPES;

	int mRow = 0;
	int mColumn = 0;
	int mPlantHealth = 300;
	int mPlantMaxHealth = 300;
	int mPlantID = NULL_PLANT_ID;
	int mEaterCount = 0;			// 正在啃食此植物的僵尸数量

protected:
	bool mIsSleeping = false;	// 白天蘑菇睡眠权威状态
	float mWakeUpTimer = 0.0f;	// 咖啡豆唤醒倒计时，单位：秒；大于 0 时仍保持睡眠
	bool mIsPreview = false;
	bool mIsSquished = false;	// 压扁残影仍参与绘制，但已退出占格、碰撞与植物行为
	float mSquishTimer = 0.0f;	// 压扁残影剩余时间，单位：秒
	Vector mSquishVisualPosition; // 进入压扁态时冻结的画面位置，不再跟随水面浮动或阵风插值
	Vector mVisualOffset;	// 视觉显示偏移
	Vector mGridMoveVisualOffset; // 阵风换格后的瞬态画面偏移；逻辑格与碰撞箱已在目标格
	Vector mGridMoveVisualStart;  // 本次平滑位移的起始偏移，用于无漂移插值
	float mGridMoveVisualTimer = 0.0f;
	float mGridMoveVisualDuration = 0.0f;
	PlantBungeeState mBungeeState = PlantBungeeState::NONE;
	int mBungeeOwnerZombieID = NULL_ZOMBIE_ID;
	Vector mBungeeVisualOffset;

public:
	static constexpr float kFlowerPotVisualLiftY = -5.0f; // 上层植物相对花盆抬升量，单位：px

	Plant(Board* board, PlantType plantType, int row, int column,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);

	~Plant() = default;
	void Start() override;
	void Update() override;
	void Draw(Graphics* g) override;	// 重写以叠加血量显示
	Vector GetVisualPosition() const override;

	int GetSortingKey() const override { return this->mRow; }

	virtual void PlantUpdate();		// 子类重写Update用这个
	/** 绘制需要夹在承载/普通层与本体前层之间的格子背景；默认植物没有这一层。 */
	virtual void DrawStackBackground(Graphics*) {}
	// 统一结算植物承伤；source 必填，使僵尸增伤只作用于僵尸来源。
	virtual void TakeDamage(int damage, DamageSource source);
	/** 当前是否能被僵尸选为啃食目标；睡莲用它实现种下后的短暂无咬保护。 */
	virtual bool CanBeEaten() const {
		return !mIsSquished && mBungeeState == PlantBungeeState::NONE;
	}
	/** 僵尸正式啃食命中时的植物侧反馈入口；默认植物不产生专属碎屑。 */
	virtual void OnZombieBite(const Vector&) {}
	/** 是否能在当前跳跃类别的判定节点阻拦僵尸；高坚果等阻拦植物覆写此接口。 */
	virtual bool BlocksZombieJump(ZombieJumpType) const { return false; }
	/** 远程索敌的高度层许可；默认植物只锁定可被地面弹丸命中的僵尸。 */
	virtual bool CanAcquireZombie(const Zombie* zombie) const;
	/** 跳跃确实被本植物阻拦后的音画反馈入口；由跳跃状态机保证每次只调用一次。 */
	virtual void OnZombieJumpBlocked(ZombieJumpType) {}
	/** 是否在强/超强台风的逐格结算中锚定整个植物格；默认植物会随阵风移动。 */
	virtual bool AnchorsPlantCellAgainstTyphoon() const { return false; }
	/**
	 * 一个相邻植物格被本植物直接挡下一格时的结算入口。
	 * showFeedback 在同一阵风首次撞击时为 true，供品种合并同帧音画而不合并逐格伤害。
	 */
	virtual void OnTyphoonPlantImpact(bool showFeedback) {}
	virtual void SaveExtraData(nlohmann::json& j) const {}
	virtual void LoadExtraData(const nlohmann::json& j) {}
	virtual void Die();
	/**
	 * 把植物变为原版压扁残影：冻结当前位置和动画、释放占格，并在渐隐后销毁。
	 * 未来巨人、冰车和投篮车只需在各自命中结算中调用本入口。
	 */
	void Squish();
	bool IsSquished() const { return mIsSquished; }
	float GetSquishTimeRemaining() const { return mSquishTimer; }
	float GetSquishRenderScaleY() const {
		return mAnimator ? mAnimator->GetRenderScaleY() : 1.0f;
	}
	Vector GetSquishVisualPosition() const { return mSquishVisualPosition; }
	/** 由 GameInfoSaver 在派生类额外数据恢复后重建压扁终态。 */
	void RestoreSquishState(float remainingSeconds, const Vector& visualPosition);
	Vector GetPosition() const;
	/**
	 * 返回不含品种静态 offset 的公共视觉锚点。
	 * 阵风换格与水面浮动都在此收口，供本体、阴影和其他附属视觉保持同步。
	 */
	Vector GetVisualAnchorPosition() const;
	/** 返回 gamedata 配置的品种静态视觉偏移，不包含任何逐帧动态量。 */
	Vector GetStaticVisualOffset() const { return mVisualOffset; }
	void SetPosition(const Vector& position);
	/**
	 * 立即把逻辑格与碰撞箱切到目标格，再用纯视觉偏移平滑追赶。
	 * 视觉偏移不入存档；滑动中读档会稳定落在已经结算的目标格。
	 */
	void MoveToGridCell(int row, int column, float visualDuration);
	/** 由蹦极僵尸建立抓取关系；成功后暂停行为并关闭碰撞。 */
	bool BeginBungeeGrab(int zombieID);
	/** 把已抓植物切到升空态；只有当前抓取者能推进关系。 */
	bool BeginBungeeLift(int zombieID);
	/** 抓取者落地阶段死亡时恢复植物行动和碰撞。 */
	void CancelBungeeGrab(int zombieID);
	/** 更新升空视觉偏移；逻辑格在真正偷走前保持不变。 */
	void SetBungeeVisualOffset(int zombieID, const Vector& offset);
	/** 供蹦极僵尸在本体与前臂之间绘制升空植物。 */
	void DrawAsBungeeCargo(Graphics* g);
	PlantBungeeState GetBungeeState() const { return mBungeeState; }
	int GetBungeeOwnerZombieID() const { return mBungeeOwnerZombieID; }
	bool IsBungeeTargeted() const {
		return mBungeeState != PlantBungeeState::NONE;
	}

	// 获取睡觉状态
	bool GetSleepState() const { return this->mIsSleeping; }
	float GetWakeUpTimeRemaining() const { return mWakeUpTimer; }
	bool IsWakingUp() const { return mWakeUpTimer > 0.0f; }

	// 是否为预览植物（选卡预览用，不参与对战逻辑）
	bool IsPreview() const { return this->mIsPreview; }

	/** 切换睡眠状态；蘑菇覆写此入口以同步睡眠/清醒动画。 */
	virtual void SetSleepState(bool sleep) { this->mIsSleeping = sleep; }
	/** 咖啡豆请求开始原版 1 秒唤醒流程；重复请求或非睡眠植物返回 false。 */
	bool BeginWakeUp(float durationSeconds = 1.0f);
	/** 存档恢复专用：只还原权威状态与表现，不播放唤醒音效或重新触发品种行为。 */
	void RestoreSleepState(bool sleep, float wakeUpTimeRemaining);

protected:
	/** 推进阵风换格的纯视觉插值；暂停时 DeltaTime 为 0，逻辑占格不受影响。 */
	void UpdateGridMoveVisual();
	/** 推进压扁残影的保留与渐隐计时，到期后销毁。 */
	void UpdateSquish();
	/** 推进咖啡豆唤醒倒计时、原版纵向弹性表现及两个音效/状态边界。 */
	void UpdateWakeUp();
	/** 按当前唤醒倒计时重建蘑菇纵向弹性表现；读档与逐帧更新共用。 */
	void ApplyWakeUpPresentation();
	/** 统一施加压扁态的暂停、碰撞、影子、占格和透明度表现。 */
	void ApplySquishedPresentation();
	/** 仅在格子仍指向自身 ID 时释放所属占格层，避免误清后来种下的植物。 */
	void ReleaseGridSlot();
	/** 雨势对正向植物行动的倍率；不包含生存攻速词条。 */
	float GetWeatherActionSpeedMultiplier() const;
	/** 仅供攻击/生产/成长/恢复计时使用，禁止替代整个 Plant::Update 的 deltaTime。 */
	float GetWeatherActionDeltaTime() const;
	/** 产光专用计时增量 = 雨势行动倍率 × 路灯花局部照明倍率。 */
	float GetSunProductionDeltaTime() const;
	/** 攻击专用组合倍率 = 生存攻速词条 × 雨势行动倍率。 */
	float GetAttackSpeedMultiplier() const;
	/** 返回血量文字相对公共视觉锚点的偏移；叠层品种可覆写以避免文字重叠。 */
	virtual Vector GetHealthTextOffset() const { return Vector(-21.0f, -11.0f); }

	// 注意： 需要判断mIsPreview，所有植物都执行
	virtual void SetupPlant();
};

#endif
