#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"
#include "../UI/UIManager.h"

class GameScene : public Scene {
public:
    GameScene();
    ~GameScene() override;

    void OnEnter() override;
    void OnExit() override;
    void Update() override;
    void Draw(SDL_Renderer* renderer) override;
    void HandleEvent(SDL_Event& event) override;

private:
	std::unique_ptr<Board> mBoard;
    UIManager uiManager_;
    SDL_Texture* background_ = nullptr;

    void SetupUI();
    void OnBackToMenuClicked();
    void OnRestartClicked();
};

#endif