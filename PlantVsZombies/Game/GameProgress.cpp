#include "GameProgress.h"
#include "GameScene.h"
#include "Board.h"
#include "SceneManager.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../Logger.h"
#include <memory>
#include <cmath>

GameProgress::GameProgress(Board* board, GameScene* gameScene)
	: mBoard(board), mGameScene(gameScene),
	mCurrentSliderValue(1.0f), mTargetSliderValue(1.0f), mLerpSpeed(1.1f)
{
	if (!board || !gameScene) {
		LOG_ERROR("GameProgress") << "初始化失败，board或者gameScene为nullptr！";
		return;
	}

	mTransform = this->AddComponent<TransformComponent>(createPosition);

	m_flagMeter = std::make_unique<FlagMeter>(createPosition, 1.0f);

	// 从资源管理器获取纹理（返回 const Texture*）
	auto& rm = ResourceManager::GetInstance();
	using namespace ResourceKeys::Textures;
	const Texture* bgTex = rm.GetTexture(IMAGE_FLAG_METER);
	const Texture* fillTex = rm.GetTexture(IMAGE_FLAG_METERFULL);
	const Texture* headTex = rm.GetTexture(IMAGE_FLAGMETER_PART_HEAD);
	const Texture* middleTex = rm.GetTexture(IMAGE_FLAGMETERLEVELPROGRESS);

	m_flagMeter->SetImages(bgTex, fillTex, headTex, middleTex);
}

GameProgress::~GameProgress()
{
	mBoard = nullptr;
	mGameScene = nullptr;
	LOG_DEBUG("GameProgress") << "~GameProgress";
}

void GameProgress::Update()
{
	GameObject::Update();
	float delta = DeltaTime::GetDeltaTime();
	if (m_flagMeter) {
		m_flagMeter->Update(delta);
	}

	mUpdateTimer += delta;

	if (mBoard && mUpdateTimer >= 0.4f)
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

void GameProgress::SetupFlags(const Texture* stickTex, const Texture* flagTex)
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

void GameProgress::LowerAllFlags(float duration)
{
	if (!m_flagMeter) return;
	// targetY=0 即基准位；RaiseFlag 会从当前升起高度平滑插值回 0，得到"下降"动画。
	int count = static_cast<int>(m_flagMeter->GetFlagCount());
	for (int i = 0; i < count; ++i) {
		m_flagMeter->RaiseFlag(i, 0.0f, duration);
	}
	// 同步重置波次记录，避免 Update 中 currentWave 与 m_lastWave 的判定残留上一轮状态
	m_lastWave = mBoard ? mBoard->mCurrentWave : 0;
}

void GameProgress::SnapProgressToCurrentWave()
{
	if (!mBoard || !m_flagMeter) return;

	int maxWave = mBoard->mMaxWave;
	if (maxWave <= 0) return;

	int currentWave = mBoard->mCurrentWave;
	float value = 1.0f - static_cast<float>(currentWave) / static_cast<float>(maxWave);

	mTargetSliderValue = value;
	mCurrentSliderValue = value;
	mUpdateTimer = 0.0f;
	m_lastWave = currentWave;
	m_flagMeter->SetProgress(mCurrentSliderValue);
}