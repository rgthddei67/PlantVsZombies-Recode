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

class Board;

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
	bool mIsSleeping = false;	// 
	bool mIsPreview = false;
	bool mIsSquished = false;	// 压扁残影仍参与绘制，但已退出占格、碰撞与植物行为
	float mSquishTimer = 0.0f;	// 压扁残影剩余时间，单位：秒
	Vector mSquishVisualPosition; // 进入压扁态时冻结的画面位置，不再跟随水面浮动或阵风插值
	Vector mVisualOffset;	// 视觉显示偏移
	Vector mGridMoveVisualOffset; // 阵风换格后的瞬态画面偏移；逻辑格与碰撞箱已在目标格
	Vector mGridMoveVisualStart;  // 本次平滑位移的起始偏移，用于无漂移插值
	float mGridMoveVisualTimer = 0.0f;
	float mGridMoveVisualDuration = 0.0f;

public:
	Plant(Board* board, PlantType plantType, int row, int column,
		AnimationType animType, float scale = 1.0f, bool isPreview = false);

	~Plant() = default;
	void Start() override;
	void Update() override;
	void Draw(Graphics* g) override;	// 重写以叠加血量显示
	Vector GetVisualPosition() const override;

	int GetSortingKey() const override { return this->mRow; }

	virtual void PlantUpdate();		// 子类重写Update用这个
	// 统一结算植物承伤；source 必填，使僵尸增伤只作用于僵尸来源。
	virtual void TakeDamage(int damage, DamageSource source);
	/** 当前是否能被僵尸选为啃食目标；睡莲用它实现种下后的短暂无咬保护。 */
	virtual bool CanBeEaten() const { return !mIsSquished; }
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
	/** 逻辑格中心叠加阵风瞬态偏移，供阴影与其他非本体视觉同步滑动。 */
	Vector GetGridVisualPosition() const { return GetPosition() + mGridMoveVisualOffset; }
	void SetPosition(const Vector& position);
	/**
	 * 立即把逻辑格与碰撞箱切到目标格，再用纯视觉偏移平滑追赶。
	 * 视觉偏移不入存档；滑动中读档会稳定落在已经结算的目标格。
	 */
	void MoveToGridCell(int row, int column, float visualDuration);

	// 获取睡觉状态
	bool GetSleepState() const { return this->mIsSleeping; }

	// 是否为预览植物（选卡预览用，不参与对战逻辑）
	bool IsPreview() const { return this->mIsPreview; }

	// 设置睡觉状态
	virtual void SetSleepState(bool sleep) { this->mIsSleeping = sleep; }

protected:
	/** 推进阵风换格的纯视觉插值；暂停时 DeltaTime 为 0，逻辑占格不受影响。 */
	void UpdateGridMoveVisual();
	/** 推进压扁残影的保留与渐隐计时，到期后销毁。 */
	void UpdateSquish();
	/** 统一施加压扁态的暂停、碰撞、影子、占格和透明度表现。 */
	void ApplySquishedPresentation();
	/** 仅在格子仍指向自身 ID 时释放上下层占格，避免误清后来种下的植物。 */
	void ReleaseGridSlot();
	/** 雨势对正向植物行动的倍率；不包含生存攻速词条。 */
	float GetWeatherActionSpeedMultiplier() const;
	/** 仅供攻击/生产/成长/恢复计时使用，禁止替代整个 Plant::Update 的 deltaTime。 */
	float GetWeatherActionDeltaTime() const;
	/** 攻击专用组合倍率 = 生存攻速词条 × 雨势行动倍率。 */
	float GetAttackSpeedMultiplier() const;

	// 注意： 需要判断mIsPreview，所有植物都执行
	virtual void SetupPlant();
};

#endif
