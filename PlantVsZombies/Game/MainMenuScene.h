#pragma once
#ifndef _MAINMENUSCENE_H
#define _MAINMENUSCENE_H

#include "../UI/GameMessageBox.h"
#include "../DeltaTime.h"
#include "SceneManager.h"
#include "Scene.h"
#include <memory>

class MainMenuButtons;

class MainMenuScene : public Scene {
public:
	~MainMenuScene() override;
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	bool mReadyToSwitchAdventureLevel = false;
	bool mReadyToSkipToSecondArea = false;
	bool mReadyToSwitchAlmanac = false;
	bool mReadyToSwitchSurvival = false;

private:
	std::unique_ptr<MainMenuButtons> mMainMenuButtons;
	std::shared_ptr<Button> mSkipToSecondAreaButton;
	std::shared_ptr<Button> mOpitionButton;
	std::shared_ptr<Button> mConsoleButton;
	std::shared_ptr<Button> mExitButton;
	std::shared_ptr<Button> mAlmanacButton;

	std::weak_ptr<GameMessageBox> mMenu;
	std::weak_ptr<GameMessageBox> mConsoleMenu;

	bool mOpenMenu = false;
	bool mOpenConsole = false;
	bool mReadyToRefreshConsole = false;

	/** 补齐第一大关进度与植物奖励，然后从 2-1 开始游戏。 */
	void SkipToSecondArea();
	/** 统一控制主菜单入口的命中状态，防止模态窗口打开时点击到背景按钮。 */
	void SetMainMenuButtonsEnabled(bool enabled);
	/** 打开原版风格的音量、画面和难度选项面板。 */
	void OpenMenu();
	/** 打开仅承载高级玩法开关的控制台设置面板。 */
	void OpenConsole();
	/** 关闭控制台设置面板并恢复主菜单输入。 */
	void CloseConsole();

protected:
	/** 构建主菜单背景与入口绘制命令；入口先于 UIManager 的模态层提交。 */
	void BuildDrawCommands() override;
};

/** 由 MainMenuScene 直接拥有的四个主入口按钮控制器。 */
class MainMenuButtons {
private:
	MainMenuScene* mMainMenuScene = nullptr;
	UIManager* mUIManager = nullptr;
	std::weak_ptr<Button> mAdventure;
	std::weak_ptr<Button> mMiniGames;
	std::weak_ptr<Button> mPizzle;
	std::weak_ptr<Button> mSurvival;

public:
	MainMenuButtons(UIManager* manager, MainMenuScene* mainMenuScene)
	{
		this->mUIManager = manager;
		this->mMainMenuScene = mainMenuScene;
	}

	/** 创建四个按钮并把生命周期交给场景的 UIManager。 */
	void Initialize()
	{
		auto adventure = mUIManager->CreateButton(Vector(545, 85), Vector(330 * 1.00f, 120 * 1.00f));
		mAdventure = adventure;
		adventure->SetAsCheckbox(false);
		adventure->SetSkipDraw(true);
		adventure->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_ADVENTURE_BUTTON,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_ADVENTURE_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_ADVENTURE_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_ADVENTURE_HIGHLIGHT);
		adventure->SetClickCallBack([this](bool) {
			DeltaTime::SetPaused(false);
			mMainMenuScene->mReadyToSwitchAdventureLevel = true;
			});

		auto minigames = mUIManager->CreateButton(Vector(545, 175), Vector(313 * 1.00f, 123 * 1.00f));
		mMiniGames = minigames;
		minigames->SetAsCheckbox(false);
		minigames->SetSkipDraw(true);
		minigames->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME_SHADOW,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME);
		auto pizzle = mUIManager->CreateButton(Vector(545, 252), Vector(286 * 1.00f, 122 * 1.00f));
		mPizzle = pizzle;
		pizzle->SetAsCheckbox(false);
		pizzle->SetSkipDraw(true);
		pizzle->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_BUTTON,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT);
		auto survival = mUIManager->CreateButton(Vector(545, 325), Vector(266 * 1.00f, 123 * 1.00f));
		mSurvival = survival;
		survival->SetAsCheckbox(false);
		survival->SetSkipDraw(true);
		survival->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW);
		survival->SetClickCallBack([this](bool) {
			DeltaTime::SetPaused(false);
			mMainMenuScene->mReadyToSwitchSurvival = true;
			});
	}

	/** 切换四个入口的命中能力；禁用时同步清除残留的悬停和按压状态。 */
	void SetEnabled(bool enabled) {
		auto setEnabled = [enabled](const std::weak_ptr<Button>& weakButton) {
			if (auto button = weakButton.lock()) {
				button->SetEnabled(enabled);
				if (!enabled) button->ForceResetHoverState();
			}
		};
		setEnabled(mAdventure);
		setEnabled(mMiniGames);
		setEnabled(mPizzle);
		setEnabled(mSurvival);
	}

	void Draw(Graphics* g) {
		if (auto adventure = mAdventure.lock())
		{
			adventure->Draw(g);
		}
		if (auto minigames = mMiniGames.lock())
		{
			minigames->Draw(g);
		}
		if (auto pizzle = mPizzle.lock())
		{
			pizzle->Draw(g);
		}
		if (auto survival = mSurvival.lock())
		{
			survival->Draw(g);
		}
	}
};

#endif
