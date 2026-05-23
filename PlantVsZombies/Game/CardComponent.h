#pragma once
#ifndef _CARD_COMPONENT_H
#define _CARD_COMPONENT_H

#include "Component.h"
#include "./CardDisplayComponent.h"
#include "./Plant/PlantType.h"
#include <memory>

class CardSlotManager;
class Card;
class GameObject;

class CardComponent : public Component {
private:
    bool mIsInChooseCardUI = false; // 是否在选卡界面
    // CardSlotManager 由另一个 GameObject 持有（跨 GameObject 引用）；
    // host 是场景级别对象，比 Card 活得久，用裸指针观察即可
    mutable GameObject* mCardSlotManagerHost = nullptr;
    // CardDisplayComponent 与本组件同属一个 GameObject，生命周期一致，直接缓存裸指针
    mutable CardDisplayComponent* mCardDisplayComponent = nullptr;
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
    bool NeedsUpdate() const override { return true; }
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

    GameObject* FindCardSlotManagerHost() const;

    CardSlotManager* GetCardSlotManager() const;
    CardDisplayComponent* GetCardDisplayComponent() const;

	void SetCardGameClick(GameObject* gameObject);    // 设置游戏界面点击回调 而不是选卡见面的
	void SetCardChooseClick(GameObject* gameObject, Card* card);  // 设置选卡界面点击回调
};

#endif
