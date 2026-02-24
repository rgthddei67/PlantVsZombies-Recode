#include "GameProgress.h"
#include "GameScene.h"
#include "Board.h"
#include "SceneManager.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include <memory>
#include <iostream>
#include <cmath>

GameProgress::GameProgress(Board* board, GameScene* gameScene)
    : mBoard(board), mGameScene(gameScene),
    mCurrentSliderValue(1.0f), mTargetSliderValue(1.0f), mLerpSpeed(5.0f)
{
    if (!board || !gameScene) {
        std::cerr << "GameProgress::GameProgress初始化失败，board或者gameScene为nullptr！" << std::endl;
        return;
    }

    mTransform = this->AddComponent<TransformComponent>(createPosition);

    m_flagMeter = std::make_unique<FlagMeter>(createPosition, 1.0f);

    // 从资源管理器获取纹理（返回 const GLTexture*）
    auto& rm = ResourceManager::GetInstance();
    using namespace ResourceKeys::Textures;
    const GLTexture* bgTex = rm.GetTexture(IMAGE_FLAG_METER);
    const GLTexture* fillTex = rm.GetTexture(IMAGE_FLAG_METERFULL);
    const GLTexture* headTex = rm.GetTexture(IMAGE_FLAGMETER_PART_HEAD);
    const GLTexture* middleTex = rm.GetTexture(IMAGE_FLAGMETERLEVELPROGRESS);

    m_flagMeter->SetImages(bgTex, fillTex, headTex, middleTex);
}

GameProgress::~GameProgress()
{
    mBoard = nullptr;
    mGameScene = nullptr;
#ifdef _DEBUG
    std::cout << "GameProgress::~GameProgress" << std::endl;
#endif
}

void GameProgress::Update()
{
    GameObject::Update();
    float delta = DeltaTime::GetDeltaTime();
    if (m_flagMeter) {
        m_flagMeter->Update(delta);
    }

    mUpdateTimer += delta;

    if (mBoard && mUpdateTimer >= 0.7f)
    {
        mUpdateTimer = 0.0f;
        int currentWave = mBoard->mCurrentWave;
        int maxWave = mBoard->mMaxWave;
        mTargetSliderValue = 1.0f - (static_cast<float>(currentWave) / static_cast<float>(maxWave));
    }

    if (m_flagMeter)
    {
        mCurrentSliderValue += (mTargetSliderValue - mCurrentSliderValue) * mLerpSpeed * delta;

        if (std::fabs(mCurrentSliderValue - mTargetSliderValue) < 0.001f)
            mCurrentSliderValue = mTargetSliderValue;

        m_flagMeter->SetProgress(mCurrentSliderValue);
    }

    // 检测波次变化，触发旗子升起
    if (mBoard) {
        int currentWave = mBoard->mCurrentWave;
        if (currentWave != m_lastWave && currentWave % 10 == 0 && currentWave != 0) {
            int flagIndex = currentWave / 10 - 1;                     // 逻辑索引（0对应10波）
            int reverseIndex = m_flagCount - 1 - flagIndex;          // 从右向左
            if (reverseIndex >= 0 && reverseIndex < m_flagCount) {
                const float RAISE_HEIGHT = -10.0f;   // 升起高度（像素）
                const float RAISE_DURATION = 1.5f;  // 动画时长（秒）
                m_flagMeter->RaiseFlag(reverseIndex, RAISE_HEIGHT, RAISE_DURATION);
            }
        }
        m_lastWave = currentWave;
    }
}

void GameProgress::Draw(Graphics* g)
{
    GameObject::Draw(g);  
    if (m_flagMeter)
        m_flagMeter->Draw(g);
}

void GameProgress::SetupFlags(const GLTexture* stickTex, const GLTexture* flagTex)
{
    if (!m_flagMeter) return;

    m_flagMeter->ClearFlags();

    int maxWave = mBoard->mMaxWave;
    int flagCount = maxWave / 10;
    m_flagCount = flagCount;
    if (flagCount <= 0) return;

    for (int i = 0; i < flagCount; ++i)
    {
        float pos = static_cast<float>(i * 10) / maxWave;
        m_flagMeter->AddFlag(stickTex, flagTex, pos);
    }
}

void GameProgress::InitializeRaisedFlags(float raiseY)
{
    if (!m_flagMeter) return;
    int raisedCount = mBoard->mCurrentWave / 10;  // 已经历的10的倍数波次数
    for (int i = 0; i < raisedCount && i < m_flagCount; ++i) {
        int reverseIndex = m_flagCount - 1 - i;   // 从右向左
        m_flagMeter->SetFlagRaiseImmediate(reverseIndex, raiseY);
    }
}