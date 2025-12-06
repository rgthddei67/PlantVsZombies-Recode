#pragma once
#ifndef _CARD_COMPONENT_H
#define _CARD_COMPONENT_H

#include "Component.h"
#include "./CardDisplayComponent.h"
#include "./Plant/PlantType.h"
#include <memory>

class CardSlotManager;

class CardComponent : public Component {
private:
    mutable std::weak_ptr<CardSlotManager> mCardSlotManager;    // 可直接被const函数修改
    mutable std::weak_ptr<CardDisplayComponent> mCardDisplayComponent;
    PlantType mPlantType = PlantType::PLANT_PEASHOOTER;
    int mSunCost = 0;
    float mCooldownTimer = 0;
    float mCooldownTime = 0;
    bool mIsReady = true;
    bool mIsSelected = false;
    bool mIsCooldown = false;

public:
    CardComponent(PlantType type, int cost, float cooldown);

    void Start() override;
    void Update() override;
    void ForceStateUpdate();

    bool IsReady() const { return mIsReady && !mIsCooldown; }
    bool IsCooldown() const { return mIsCooldown; }
    void StartCooldown();
    void SetSelected(bool selected);
    bool IsSelected() const { return mIsSelected; }

    PlantType GetPlantType() const { return mPlantType; }
    int GetSunCost() const { return mSunCost; }
    float GetCooldownProgress() const;
    CardState GetCardState() const;

    std::shared_ptr<CardSlotManager> FindCardSlotManager() const;

    std::shared_ptr<CardSlotManager> GetCardSlotManager() const;
    std::shared_ptr<CardDisplayComponent> GetCardDisplayComponent() const;
};

#endif