#pragma once
#ifndef _CARD_COMPONENT_H
#define _CARD_COMPONENT_H

#include "Component.h"
#include "./CardDisplayComponent.h"
#include "./Plant/PlantType.h"
#include <memory>

class CardSlotManager;
class Card;

class CardComponent : public Component {
private:
    bool mIsInChooseCardUI = false; // 是否在选卡界面
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
    float GetCooldownTimer() const { return mCooldownTimer; }
    float GetCooldownTime()  const { return mCooldownTime;  }
    float GetCooldownProgress() const;
    CardState GetCardState() const;
    void RestoreCooldown(float timer, float time);

	bool GetIsInChooseCardUI() const { return mIsInChooseCardUI; }
	void SetIsInChooseCardUI(bool isInChooseCardUI) { mIsInChooseCardUI = isInChooseCardUI; }

    std::shared_ptr<CardSlotManager> FindCardSlotManager() const;

    std::shared_ptr<CardSlotManager> GetCardSlotManager() const;
    std::shared_ptr<CardDisplayComponent> GetCardDisplayComponent() const;

	void SetCardGameClick(std::shared_ptr<GameObject> gameObject);    // 设置游戏界面点击回调 而不是选卡见面的
	void SetCardChooseClick(std::shared_ptr<GameObject> gameObject, std::shared_ptr<Card> card);  // 设置选卡界面点击回调
};

#endif