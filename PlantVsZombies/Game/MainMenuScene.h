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
    void Draw(Graphics* g) override;

    bool mReadyToSwitchAdventureLevel = false;

private:
    std::shared_ptr<class GameButton> mGameButton;
    std::shared_ptr<Button> mOpitionButton;
    std::weak_ptr<Button> mExitButton;

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
        auto adventure = mUIManager->CreateButton(Vector(580, 90), Vector(330 * 0.82f, 120 * 0.82f));
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

        auto minigames = mUIManager->CreateButton(Vector(580, 180), Vector(313 * 0.82f, 123 * 0.82f));
        mMiniGames = minigames;
        minigames->SetAsCheckbox(false);
        minigames->SetSkipDraw(true);
        minigames->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME_SHADOW,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_GAME);
        auto pizzle = mUIManager->CreateButton(Vector(580, 270), Vector(286 * 0.82f, 122 * 0.82f));
        mPizzle = pizzle;
        pizzle->SetAsCheckbox(false);
        pizzle->SetSkipDraw(true);
        pizzle->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_BUTTON,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT);
        auto survival = mUIManager->CreateButton(Vector(580, 350), Vector(266 * 0.82f, 123 * 0.82f));
        mSurvival = survival;
        survival->SetAsCheckbox(false);
        survival->SetSkipDraw(true);
        survival->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW,
            ResourceKeys::Textures::IMAGE_SELECTORSCREEN_SURIVAL_SHADOW);
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