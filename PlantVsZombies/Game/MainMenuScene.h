#pragma once
#ifndef _MAINMENUSCENE_H
#define _MAINMENUSCENE_H

#include "../UI/GameMessageBox.h"
#include "../DeltaTime.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "Scene.h"

class MainMenuScene : public Scene {
public:
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	bool mReadyToSwitchAdventureLevel = false;
	bool mReadyToSwitchAlmanac = false;
	bool mReadyToSwitchSurvival = false;

private:
	class GameButton* mGameButton = nullptr;   // 所有权在 GameObjectManager
	std::shared_ptr<Button> mOpitionButton;
	std::shared_ptr<Button> mExitButton;
	std::shared_ptr<Button> mAlmanacButton;

	std::weak_ptr<GameMessageBox> mMenu;

	bool mOpenMenu = false;

	void OpenMenu();

protected:
	void BuildDrawCommands() override;
};

// 主菜单4个按钮
class GameButton : public GameObject {
private:
	MainMenuScene* mMainMenuScene = nullptr;
	UIManager* mUIManager = nullptr;
	std::weak_ptr<Button> mAdventure;
	std::weak_ptr<Button> mMiniGames;
	std::weak_ptr<Button> mPizzle;
	std::weak_ptr<Button> mSurvival;

public:
	GameButton(UIManager* manager, MainMenuScene* mainMenuScene) : GameObject(ObjectType::OBJECT_UI)
	{
		this->mUIManager = manager;
		this->mMainMenuScene = mainMenuScene;
	}

	void Start() override
	{
		GameObject::Start();
		auto adventure = mUIManager->CreateButton(Vector(565, 85), Vector(330 * 0.95f, 120 * 0.95f));
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

		auto minigames = mUIManager->CreateButton(Vector(565, 172), Vector(313 * 0.95f, 123 * 0.95f));
		mMiniGames = minigames;
		minigames->SetAsCheckbox(false);
		minigames->SetSkipDraw(true);
		minigames->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME_SHADOW,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME);
		auto pizzle = mUIManager->CreateButton(Vector(565, 248), Vector(286 * 0.95f, 122 * 0.95f));
		mPizzle = pizzle;
		pizzle->SetAsCheckbox(false);
		pizzle->SetSkipDraw(true);
		pizzle->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_BUTTON,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT);
		auto survival = mUIManager->CreateButton(Vector(565, 320), Vector(266 * 0.95f, 123 * 0.95f));
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

	void SetEnabled(bool enabled) {
		if (auto btn = mAdventure.lock()) btn->SetEnabled(enabled);
		if (auto btn = mMiniGames.lock()) btn->SetEnabled(enabled);
		if (auto btn = mPizzle.lock()) btn->SetEnabled(enabled);
		if (auto btn = mSurvival.lock()) btn->SetEnabled(enabled);
	}

	void Draw(Graphics* g) override {
		GameObject::Draw(g);
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