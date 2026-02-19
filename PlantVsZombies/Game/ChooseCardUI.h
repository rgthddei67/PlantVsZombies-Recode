#pragma once
#ifndef _CHOOSECARDUI_H
#define _CHOOSECARDUI_H
#include "GameObject.h"
#include "Card.h"
#include <vector>
#include <memory>

class Button;
class GameScene;

class ChooseCardUI : public GameObject {
public:
    ChooseCardUI(GameScene* gameScene);
    ~ChooseCardUI();

    void Draw(SDL_Renderer* renderer) override;

    // 后续添加卡牌的方法
    void AddCard(PlantType type);

	void RemoveCard(std::shared_ptr<Card> card);

	void RemoveAllCards();

    std::shared_ptr<TransformComponent> GetTransformComponent() const {
        return mTransform.lock();
    }

    Vector GetPosition() const {
        return GetTransformComponent()->GetWorldPosition();
    }

    // 切换卡牌选中状态，返回是否选中
    bool ToggleCardSelection(std::shared_ptr<Card> card);
    // 判断卡牌是否已选中
    bool IsCardSelected(std::shared_ptr<Card> card) const;
    // 获取所有选中的卡牌
    const std::vector<std::shared_ptr<Card>>& GetSelectedCards() const { return mSelectedCards; }
    // 获取“一起摇滚吧”按钮
    std::shared_ptr<Button> GetButton() const { return mButton; }
	// 添加所有卡牌
    void AddAllCard();
	// 转换卡牌所有权给卡槽管理器
    void TransferSelectedCardsTo(CardSlotManager* manager);

private:
    SDL_Texture* mCardUITexture = nullptr;
	GameScene* mGameScene = nullptr;

    std::weak_ptr<TransformComponent> mTransform;
    std::shared_ptr<Button> mButton;

    std::vector<std::shared_ptr<Card>> mCards;  // 存储选卡界面的卡牌
    std::vector<std::shared_ptr<Card>> mSelectedCards;   // 存储选中的卡牌对象

    static constexpr int MAX_SELECTED = 10;              // 最大选择数量
    static constexpr float SLOT_START_X = 192;                  // 槽位起始 X
    static constexpr float SLOT_START_Y = -1;                    // 槽位起始 Y
    static constexpr int SLOT_SPACING = CARD_WIDTH + 3;       // 槽位间距

    static constexpr int MAX_CARDS_PER_ROW = 8;      // 每行最多8张
    static constexpr int CARD_HORIZONTAL_SPACING = 1; // 水平间距
    static constexpr int CARD_VERTICAL_SPACING = 4;   // 垂直间距（
    static constexpr float START_X = 250;                 // 第一张卡牌的起始X坐标
    static constexpr float START_Y = 115;                // 第一行起始Y坐标

    // 更新所有卡牌的目标位置（根据选中状态）
    void UpdateTargetPositions();
};

#endif