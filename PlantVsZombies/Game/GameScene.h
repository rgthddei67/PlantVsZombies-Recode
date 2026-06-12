#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"
#include <unordered_map>
#include <utility>

class ChooseCardUI;
class CardSlotManager;
class GameProgress;
class ShovelBank;

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

	// ---- AutoTest 钩子（只读访问，不改变任何流程）----
	Board* GetBoard() const { return mBoard.get(); }
	ChooseCardUI* GetChooseCardUI() const { return mChooseCardUI; }
	// 选卡界面是否就绪（卡牌已铺开、"一起摇滚吧"可点）
	bool IsChooseCardReady() const {
		return mCurrentStage == IntroStage::COMPLETE && mChooseCardUI != nullptr;
	}

	GameProgress* GetGameProgress() const;

	void GameOver();

	// 生存模式：一轮清空后重新进入选卡子流程（同会话轮间，轻量路径）
	void BeginSurvivalCardSelect();

	void ShowSunCount();

	// 显示红色大字指示
	void ShowPrompt(const std::string& textureKey,
		float appearDur = 1.0f,
		float holdDur = 3.0f,
		float fadeDur = 1.0f);

	void SetReadyToBackMenu() { mReadyToBackMenu = true; }

	void ShowShovel();

	// 生存轮间空槽重选的冷却快照存取：该快照是选卡阶段"仍在冷却卡牌"冷却进度的唯一载体
	// （卡槽此刻已被 ClearAllCards 清空），必须能被存档持久化，否则选卡界面退出重进冷却会清零。
	const std::unordered_map<PlantType, std::pair<float, float>>& GetSurvivalCardCooldowns() const {
		return mSurvivalCardCooldowns;
	}
	void SetSurvivalCardCooldowns(std::unordered_map<PlantType, std::pair<float, float>> cooldowns) {
		mSurvivalCardCooldowns = std::move(cooldowns);
	}

protected:
	void BuildDrawCommands() override;

private:
	std::unique_ptr<Board> mBoard = nullptr;
	std::weak_ptr<Button> mMainMenuButton;
	std::weak_ptr<Button> mSpeedSettingsButton;
	ShovelBank* mShovelUI = nullptr;   // 所有权在 GameObjectManager
	std::weak_ptr<GameMessageBox> mMenu;
	CardSlotManager* mCardSlotManager = nullptr;  // 由 CardUI GameObject 持有 unique_ptr，本字段仅缓存指针
	ChooseCardUI* mChooseCardUI = nullptr;        // 所有权在 GameObjectManager
	GameProgress* mGameProgress = nullptr;        // 所有权在 GameObjectManager

	bool mOpenMenu = false;
	bool mOpenRestartMenu = false;
	bool mOpenQuitMenu = false;

	bool mReadyToBackMenu = false;
	bool mReadyToRestart = false;
	bool mSurvivalRoundTransition = false;  // true=正处于同会话轮间转场（ChooseCardComplete 走轻量路径）
	bool mGameUiRegistered = false;         // 防止 ZombieNumber/LevelName/Difficulty 绘制命令重复注册
	bool mPendingSurvivalSave = false;      // 轮清后延后一帧存档（避开濒死僵尸尚未被清理而被误序列化）
	// 轮间空槽重选时，快照冷却中卡牌的 {植物类型 → (已计时, 总时长)}，选完后还原到重选回的同种卡
	std::unordered_map<PlantType, std::pair<float, float>> mSurvivalCardCooldowns;
	bool mLendToAlmanacScene = false;

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
	void RegisterSurvivalGameUiOnce();
};

#endif