#include "GameScene.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include <iostream>

GameScene::GameScene() {
    std::cout << "游戏场景创建" << std::endl;
}

GameScene::~GameScene() {
    std::cout << "游戏场景销毁" << std::endl;
}

void GameScene::OnEnter() {
    std::cout << "进入游戏场景" << std::endl;

    // 加载背景
	// TODO: 根据传入的Background参数选择不同背景
	background_ = ResourceManager::GetInstance().GetTexture("IMAGE_background_day");
    if (!background_) {
        std::cerr << "无法加载游戏背景" << std::endl;
    }

    mBoard = std::make_unique<Board>();

    SetupUI();
}

void GameScene::OnExit() {
    std::cout << "退出游戏场景" << std::endl;
	mBoard.reset();
    uiManager_.ClearAll();
}

void GameScene::Update() {
    if (mBoard)
    {
        mBoard->Update();
    }
    uiManager_.UpdateAll(nullptr);
}

void GameScene::Draw(SDL_Renderer* renderer) 
{
    // 绘制背景
    if (background_) {
        int texWidth, texHeight;
        SDL_QueryTexture(background_, nullptr, nullptr, &texWidth, &texHeight);

        int displayX = -215;   
        int displayY = 0;   

        SDL_Rect destRect = {
            displayX,        
            displayY,     
            texWidth,      
            texHeight        
        };

        SDL_RenderCopy(renderer, background_, nullptr, &destRect);
    }
    else {
        SDL_SetRenderDrawColor(renderer, 0, 100, 0, 255);
        SDL_RenderClear(renderer);
    }

    if (mBoard) {
        mBoard->Draw(renderer);
    }

    uiManager_.DrawAll(renderer);
}

void GameScene::HandleEvent(SDL_Event& event) {
    uiManager_.ProcessMouseEvent(&event, nullptr);
	// TODO: 游戏内键盘事件处理
}

void GameScene::SetupUI() {
    /*
    // 返回主菜单按钮
    auto backButton = uiManager_.CreateButton(Vector(700, 30), Vector(80, 30));
    backButton->SetText(u8"返回");
    backButton->SetClickCallBack([this]() { OnBackToMenuClicked(); });

    // 重新开始按钮
    auto restartButton = uiManager_.CreateButton(Vector(700, 80), Vector(80, 30));
    restartButton->SetText(u8"重玩");
    restartButton->SetClickCallBack([this]() { OnRestartClicked(); });
    */
}

void GameScene::OnBackToMenuClicked() {
    std::cout << "返回主菜单" << std::endl;
    /*
    // 保存游戏数据（如果需要）
    int score = 100; // 示例分数
    SceneManager::GetInstance().SetGlobalData("last_score", std::to_string(score));

    SceneManager::GetInstance().SwitchTo("MainMenuScene");
    */
}

void GameScene::OnRestartClicked() {
    std::cout << "重新开始游戏" << std::endl;
    // 重新初始化游戏状态
    mBoard = std::make_unique<Board>();
}