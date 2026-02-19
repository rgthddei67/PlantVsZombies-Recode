#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"

class ChooseCardUI;
class CardSlotManager;

// 开场动画阶段
enum class IntroStage {
    BACKGROUND_MOVE,   // 背景移动
    SEEDBANK_SLIDE,    // 种子槽滑落
    COMPLETE,           // 完成，显示阳光
	READY_SET_PLANT,    // 准备种植植物，选好卡了
	FINISH			  // 最终完成
};

class GameScene : public Scene {
public:
    GameScene();
    ~GameScene() override;

    void OnEnter() override;
    void OnExit() override;
    void Update() override;
	void BuildDrawCommands() override;

	void ChooseCardComplete();  // 选卡完成

private:
	std::unique_ptr<Board> mBoard;
	std::shared_ptr<CardSlotManager> mCardSlotManager;
    std::shared_ptr<ChooseCardUI> mChooseCardUI;

    IntroStage mCurrentStage = IntroStage::BACKGROUND_MOVE;

	float mStartX = 0.0f;          // BackGround初始X坐标
	float mGameStartX = -50.0f;          // BackGround动画在选完卡后的坐标
	float mCurrectSceneX = 0.0f;     // BackGround当前X坐标
    float mTargetSceneX = -300.0f;         // BackGround要到达的X坐标
	bool mHasEnter = false;             // 是否已经进入场景（用于控制进入动画只播放一次）
    float mAnimDuration = 3.0f;          // 动画总时长（秒）
    float mAnimElapsed = 0.0f;           // 动画已进行时间

    // 种子槽动画参数
    float mSeedbankAnimDuration = 0.8f;   // 滑落时长
    float mSeedbankAnimElapsed = 0.0f;
    bool mSeedbankAdded = false;           // 是否已添加纹理
    bool mSunCounterRegistered = false;    // 是否已注册阳光绘制

    // 选卡界面动画
    float mChooseCardUIAnimDuration = 1.0f;          // 动画时长1秒
    float mChooseCardUIAnimElapsed = 0.0f;
    Vector mChooseCardUIStartPos = Vector(240.0f, 800.0f);   // 起始位置（屏幕下方）
    Vector mChooseCardUITargetPos = Vector(240.0f, 80.0f);   // 目标位置（屏幕内）
    bool mChooseCardUIMoving = false;                // 是否正在移动

    // 背景回移动画
    float mReadyAnimDuration = 3.0f;
    float mReadyAnimElapsed = 0.0f;
    Vector mReadyStartPos;

    void OnBackToMenuClicked();
    void OnRestartClicked();
};

#endif