#pragma once
#ifndef _CARD_H
#define _CARD_H

#include "GameObject.h"
#include "CardComponent.h"
#include "./Plant/PlantType.h"
#include "ClickableComponent.h"
#include "TransformComponent.h"

constexpr float CARD_SCALE = 0.55f; // 卡牌缩放比例
constexpr int CARD_WIDTH = static_cast<int>(100 * CARD_SCALE); // 宽度
constexpr int CARD_HEIGHT = static_cast<int>(140 * CARD_SCALE); // 高度


class Card : public GameObject {
public:
    Card(PlantType plantType, int sunCost, float cooldown, bool isInChooseCardUI = false);

    // 便捷方法
    std::shared_ptr<CardComponent> GetCardComponent() { return GetComponent<CardComponent>(); }
    std::shared_ptr<TransformComponent> GetTransform() { return mTransform.lock(); }
    std::shared_ptr<CardDisplayComponent> GetDisplay() { return GetComponent<CardDisplayComponent>(); }
	bool GetIsInChooseCardUI() const { return mIsInChooseCardUI; }
	void SetIsInChooseCardUI(bool isInChooseCardUI) { mIsInChooseCardUI = isInChooseCardUI; }

    void SetOriginalPosition(const Vector& pos) { m_originalPos = pos; }
    Vector GetOriginalPosition() const { return m_originalPos; }

    void SetTargetPosition(const Vector& target);
    bool IsMoving() const { return m_isMoving; }

    void Update() override;

private:
	std::weak_ptr<TransformComponent> mTransform;
    bool mIsInChooseCardUI = false;     // 是否是在选卡界面中的卡牌

    Vector m_originalPos;      // 原始位置（在选卡界面中的固定位置）
    Vector m_targetPos;        // 目标位置（用于动画）
    bool m_isMoving = false;
    float m_moveSpeed = 600.0f; // 移动速度

    void SetupComponents(PlantType plantType, int sunCost, float cooldown);
};

#endif