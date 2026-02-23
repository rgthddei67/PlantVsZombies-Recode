#pragma once
#include "GameObject.h"
#include "FlagMeter.h"  
#include <memory>
#include "TransformComponent.h"

class Board;
class GameScene;

class GameProgress : public GameObject
{
public:
    GameProgress(Board* board, GameScene* gameScene);
    ~GameProgress();

    void Update() override;
    void Draw(SDL_Renderer* renderer) override; 

    // 根据最大波数生成旗子，使用传入的图片键
    void SetupFlags(SDL_Texture* stickTex, SDL_Texture* flagTex);

    // 初始化升起状态（根据当前波数直接设置已升起的旗子）
    void InitializeRaisedFlags(float raiseY);

private:
    Board* mBoard;
    GameScene* mGameScene;
    float mUpdateTimer = 0.0f;

    float mCurrentSliderValue = 1.0f;   // 当前显示进度
    float mTargetSliderValue = 1.0f;    // 目标进度
    float mLerpSpeed = 0.001f;             // 插值速度

    int m_lastWave = 0;               // 上一帧波数，用于检测波次变化
    int m_flagCount = 0;              // 旗子总数

    Vector createPosition = Vector(890, 575);

    std::unique_ptr<FlagMeter> m_flagMeter;
    std::weak_ptr<TransformComponent> mTransform;
};