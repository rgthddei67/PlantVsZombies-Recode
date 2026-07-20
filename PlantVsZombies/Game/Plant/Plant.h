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
	Vector mVisualOffset;	// 视觉显示偏移

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
	virtual void SaveExtraData(nlohmann::json& j) const {}
	virtual void LoadExtraData(const nlohmann::json& j) {}
	void Die();
	Vector GetPosition() const;
	void SetPosition(const Vector& position);

	// 获取睡觉状态
	bool GetSleepState() const { return this->mIsSleeping; }

	// 是否为预览植物（选卡预览用，不参与对战逻辑）
	bool IsPreview() const { return this->mIsPreview; }

	// 设置睡觉状态
	virtual void SetSleepState(bool sleep) { this->mIsSleeping = sleep; }

protected:
	// 注意： 需要判断mIsPreview，所有植物都执行
	virtual void SetupPlant();
};

#endif
