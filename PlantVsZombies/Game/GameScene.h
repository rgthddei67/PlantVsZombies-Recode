#pragma once
#ifndef _GAMESCENE_H
#define _GAMESCENE_H
#include "Scene.h"
#include "../Game/Board.h"

class GameScene : public Scene {
public:
    GameScene();
    ~GameScene() override;

    void OnEnter() override;
    void OnExit() override;
    void Update() override;
    void Draw(SDL_Renderer* renderer) override;
    void HandleEvent(SDL_Event& event, InputHandler& input) override;

private:
	std::unique_ptr<Board> mBoard;

    void OnBackToMenuClicked();
    void OnRestartClicked();
};

#endif