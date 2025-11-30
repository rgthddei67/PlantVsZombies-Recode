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
    Card(PlantType plantType, int sunCost, float cooldown);

    // 便捷方法
    std::shared_ptr<CardComponent> GetCardComponent() { return GetComponent<CardComponent>(); }
    std::shared_ptr<TransformComponent> GetTransform() { return GetComponent<TransformComponent>(); }
    std::shared_ptr<CardDisplayComponent> GetDisplay() { return GetComponent<CardDisplayComponent>(); }

private:
    void SetupComponents(PlantType plantType, int sunCost, float cooldown);
};

#endif