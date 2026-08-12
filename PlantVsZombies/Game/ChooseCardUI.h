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

	void Update() override;
	void Draw(Graphics* g) override;

	// 后续添加卡牌的方法
	void AddCard(PlantType type);

	void RemoveCard(Card* card);

	void RemoveAllCards();

	TransformComponent* GetTransformComponent() const {
		return mTransform;
	}

	Vector GetPosition() const {
		return GetTransformComponent()->GetPosition();
	}
	/** 设置选卡面板逻辑坐标，并同步依附于面板右上角的按钮。 */
	void SetPosition(const Vector& position);

	// 切换卡牌选中状态，返回是否选中
	bool ToggleCardSelection(Card* card);
	// 判断卡牌是否已选中
	bool IsCardSelected(Card* card) const;
	// 获取所有选中的卡牌
	const std::vector<Card*>& GetSelectedCards() const { return mSelectedCards; }
	/** 按玩家点击顺序导出当前选择，用于提交后记录上一次卡组。 */
	std::vector<PlantType> GetSelectedCardTypes();
	// 获取“一起摇滚吧”按钮
	std::shared_ptr<Button> GetButton() const { return mButton.lock(); }
	// 获取面板右上角的“上次选卡”按钮
	std::shared_ptr<Button> GetRestoreButton() const { return mRestoreButton.lock(); }
	/** 用当前仍拥有且已注册的卡恢复上一次选择，并复用卡片目标位置动画。 */
	bool RestoreLastSelectedCards();
	// 添加所有卡牌
	void AddAllCard();
	// 转换卡牌所有权给卡槽管理器
	void TransferSelectedCardsTo(CardSlotManager* manager);
	// 按植物类型查找选卡界面中的卡牌（AutoTest 程序化选卡用）；找不到返回 nullptr
	Card* FindCardByType(PlantType type);
	/** 返回满配卡组最后一张卡的右边缘，供卡槽底板按真实容量收口。 */
	static float GetGameSlotRightEdge();

private:
	const Texture* mCardUITexture = nullptr;
	GameScene* mGameScene = nullptr;

	TransformComponent* mTransform = nullptr;
	std::weak_ptr<Button> mButton;
	std::weak_ptr<Button> mRestoreButton;

	std::vector<Card*> mCards;  // 存储选卡界面的卡牌（观察者，所有权在 GameObjectManager）
	std::vector<Card*> mSelectedCards;   // 存储选中的卡牌对象

	static constexpr int MAX_SELECTED = 11;              // 最大选择数量
	static constexpr float SLOT_START_X = 195;                  // 槽位起始 X 屏幕坐标
	static constexpr float SLOT_START_Y = -1;                    // 槽位起始 Y 屏幕坐标
	static constexpr int SLOT_SPACING = CARD_WIDTH + 3;       // 槽位间距

	static constexpr int MAX_CARDS_PER_ROW = 8;      // 每行最多8张
	static constexpr int CARD_HORIZONTAL_SPACING = 3; // 水平间距
	static constexpr int CARD_VERTICAL_SPACING = 4;   // 垂直间距（
	static constexpr float START_X = 210;                 // 第一张卡牌的起始X坐标 屏幕坐标
	static constexpr float START_Y = 115;                // 第一行起始Y坐标  屏幕坐标

	// 更新所有卡牌的目标位置（根据选中状态）
	void UpdateTargetPositions();
	void SyncRestoreButtonPosition();
	void RefreshRestoreButtonState();
	std::vector<Card*> ResolveRestorableCards();
};

#endif
