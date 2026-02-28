#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"

class ChooseCardUI;
class CardSlotManager;
class GameProgress;

constexpr float mBackgroundY = -50;

// 一大波、最后一波
enum class PromptStage {
    NONE,
    APPEAR,
    HOLD,
    FADE_OUT
};

// 开场动画阶段
enum class IntroStage {
    BACKGROUND_MOVE,   // 背景移动
    SEEDBANK_SLIDE,    // 种子槽滑落
    COMPLETE,           // 完成，显示阳光
    READY_SET_PLANT,    // 准备种植植物，选好卡了
    FINISH              // 最终完成
};

// 提示动画数据
struct PromptAnimation {
    bool active = false;
    PromptStage stage = PromptStage::NONE;
    float timer = 0.0f;
    float scale = 1.0f;
    Uint8 alpha = 255;
    std::string textureKey;      // 纹理的资源键
    float appearDuration = 1.0f; // 出现阶段时长
    float holdDuration = 3.0f;    // 停留阶段时长
    float fadeDuration = 1.0f;    // 消失阶段时长
};

class GameScene : public Scene {
public:
    GameScene();
    ~GameScene() override;

    void OnEnter() override;
    void OnExit() override;
    void Update() override;

    void ChooseCardComplete();  // 选卡完成

    std::shared_ptr<GameProgress> GetGameProgress() const;

    void GameOver();

    // 显示红色大字指示
    void ShowPrompt(const std::string& textureKey,
        float appearDur = 1.0f,
        float holdDur = 3.0f,
        float fadeDur = 1.0f);

protected:
    void BuildDrawCommands() override;

private:
    std::unique_ptr<Board> mBoard = nullptr;
    std::weak_ptr<Button> mMainMenuButton;
    std::weak_ptr<GameMessageBox> mMenu;
    std::shared_ptr<CardSlotManager> mCardSlotManager = nullptr;
    std::shared_ptr<ChooseCardUI> mChooseCardUI = nullptr;
    std::shared_ptr<GameProgress> mGameProgress = nullptr;

    bool mOpenMenu = false;
    bool mOpenRestartMenu = false;
    bool mOpenQuitMenu = false;

    bool mReadyToBackMenu = false;
    bool mReadyToRestart = false;

    IntroStage mCurrentStage = IntroStage::BACKGROUND_MOVE;

    float mStartX = -250.0f;          // BackGround初始X坐标
    float mGameStartX = -250.0f;          // BackGround动画在选完卡后的坐标
    float mCurrectSceneX = -250.0f;     // BackGround当前X坐标
    float mTargetSceneX = -700.0f;         // BackGround要到达的X坐标
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
    Vector mChooseCardUIStartPos = Vector(200.0f, 800.0f);   // 起始位置（屏幕下方）
    Vector mChooseCardUITargetPos = Vector(200.0f, 80.0f);   // 目标位置（屏幕内）
    bool mChooseCardUIMoving = false;                // 是否正在移动

    // 背景回移动画
    float mReadyAnimDuration = 3.0f;
    float mReadyAnimElapsed = 0.0f;
    Vector mReadyStartPos;

    PromptAnimation mPrompt;

    void OpenMenu();
    void OpenRestartMenu();
    void OpenQuitMenu();
};

#endif