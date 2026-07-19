#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"
#include "Perk/PerkType.h"
#include <unordered_map>
#include <utility>
#include <vector>

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
	// 覆写以支持屏幕抖动：抖动经相机（projView）作用于全部绘制管线，
	// 含不消费变换栈的 reanim/字形 GPU instancing 快路径（详见 .cpp 实现注释）
	void Draw(Graphics* g) override;

	void ChooseCardComplete();  // 选卡完成

	// ---- AutoTest 钩子 ----
	// GetBoard / GetChooseCardUI / IsChooseCardReady 本身只读；
	// AutoTest 经由它们配合 ChooseCardComplete()（与 UI 共用）驱动选卡流程。
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

	static constexpr int SURVIVAL_PERK_PICKS_PER_ROUND = 2;
	static constexpr int SURVIVAL_PERK_REFRESHES_PER_ROUND = 3;
	void BeginSurvivalPerkSelect();          // 轮清后开始最多两次成对词条选择（选卡之前）
	void ApplyPerkSelection(int index);      // 合法 index 应用一对；负数/越界只放弃当前一次机会
	bool RefreshSurvivalPerkSelection();     // 消耗整轮共享的一次刷新额度并重抽当前候选；不可刷新时返回 false
	void OpenPerkView();                      // 生存模式：弹出已选词条查看面板（固定面板+字号自动缩放，打开即暂停）
	void ClosePerkView();                     // 关闭词条查看面板并恢复
	void RenderPerkViewPage();                // 按 mPerkViewPage 重建词条查看面板（分页；OpenPerkView 主体）
	// AutoTest 内省接口（对真实游戏无副作用）
	bool IsPerkSelectActive() const { return mSurvivalPerkSelectActive; }
	int GetPerkCurrentPick() const { return mSurvivalPerkSelectActive ? mSurvivalPerkStepsCompleted + 1 : 0; }
	int GetPerkStepsCompleted() const { return mSurvivalPerkStepsCompleted; }
	int GetPerkPicksCompleted() const { return mSurvivalPerkPicksCompleted; }
	int GetPerkRefreshesRemaining() const { return mSurvivalPerkRefreshesRemaining; }
	const std::vector<PerkPairing>& GetCurrentPerkOffer() const { return mCurrentPerkOffer; }

	void ShowSunCount();

	// 显示红色大字指示
	void ShowPrompt(const std::string& textureKey,
		float appearDur = 1.0f,
		float holdDur = 3.0f,
		float fadeDur = 1.0f);

	// 全屏白闪（寒冰菇全场冻结的瞬间反馈）：alpha 从峰值线性衰减到 0
	void ShowScreenFlash(float duration = 0.5f);

	void SetReadyToBackMenu() { mReadyToBackMenu = true; }

	void ShowShovel();

	// 生存轮间空槽重选的冷却快照存取：词条选择期间退出也会保存，因此必须在进入词条页时
	// 提前捕获；随后旧卡槽清空后，它是仍在冷却卡牌进度的唯一载体。
	const std::unordered_map<PlantType, std::pair<float, float>>& GetSurvivalCardCooldowns() const {
		return mSurvivalCardCooldowns;
	}
	void SetSurvivalCardCooldowns(std::unordered_map<PlantType, std::pair<float, float>> cooldowns) {
		mSurvivalCardCooldowns = std::move(cooldowns);
	}

protected:
	void BuildDrawCommands() override;

private:
	// 按当前 mSurvivalPerkStepsCompleted 重新 roll 并构建第 N/2 次选择框。
	void RenderSurvivalPerkSelectStep();
	// 立即停用并延迟销毁当前选择框，避免刷新或进入下一步时出现一帧双框。
	void CloseSurvivalPerkSelectBox();

	std::unique_ptr<Board> mBoard = nullptr;
	std::weak_ptr<Button> mMainMenuButton;
	std::weak_ptr<Button> mSpeedSettingsButton;
	ShovelBank* mShovelUI = nullptr;   // 所有权在 GameObjectManager
	std::weak_ptr<GameMessageBox> mMenu;
	std::weak_ptr<GameMessageBox> mPerkSelectBox;
	std::vector<PerkPairing>      mCurrentPerkOffer;        // 本轮展示的配对（AutoTest dump 用）
	bool                          mSurvivalPerkSelectActive = false;
	int                           mSurvivalPerkStepsCompleted = 0; // 本轮已消耗的选择机会数（选择或放弃均 +1）
	int                           mSurvivalPerkPicksCompleted = 0; // 本轮已成功选择的正负配对数（0~2）
	int                           mSurvivalPerkRefreshesRemaining = 0; // 两次选择共享；每轮开始重置为 3
	std::weak_ptr<Button>         mPerkViewButton;          // 生存模式右上角「词条」按钮（仅生存关创建）
	std::weak_ptr<GameMessageBox> mPerkViewBox;             // 词条查看面板
	bool                          mPerkViewActive = false;  // 面板打开中（守卫暂停叠态）
	int                           mPerkViewPage   = 0;      // 词条查看面板当前页（0-based）

	// ---- 开发者模式（-develop，D 键面板）----
	std::weak_ptr<GameMessageBox> mDevPanelBox;
	bool mDevPanelActive    = false;  // 面板打开中（守卫暂停叠态）
	bool mDevSpawnMode      = false;  // 召唤放置模式（选好类型后点草坪生成）
	bool mDevHintRegistered = false;  // 放置模式提示绘制命令已注册
	int  mDevZombieIndex    = 0;      // kDevZombieTable 下标
	int  mDevLevelSel       = 1;      // 面板选中的关卡号
	int  mDevPendingLevel   = -1;     // >=0 时 Update 尾部执行跳关（回调内不可 SwitchTo 销毁自身）
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

	// 全屏白闪剩余/总时长（秒）；timer<=0 即不激活、不注册额外状态
	float mScreenFlashTimer = 0.0f;
	float mScreenFlashDuration = 0.5f;

	// 屏幕抖动上一帧是否占用了相机（true 时抖动归零后须把相机复位一次；
	// 平时不触碰相机，避免与开场动画的 SetCameraPosition(camX,0) 打架）
	bool mShakeCameraApplied = false;

	void OpenMenu();
	void OpenRestartMenu();
	void OpenQuitMenu();

	// ---- 开发者模式 ----
	void OpenDevPanel();
	void CloseDevPanel();
	void RenderDevPanel();        // 状态变化即整体重建（autoClose 帧末自毁旧盒）
	void BeginDevSpawnMode();     // 进入召唤放置模式
	void DevJumpToLevel();        // 置 pending，Update 尾部切关
	void DevTriggerNextWave();    // 出波倒计时清零
	void RegisterSurvivalGameUiOnce();
};

#endif
