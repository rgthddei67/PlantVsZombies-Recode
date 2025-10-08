#include "GameScene.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include <iostream>

GameScene::GameScene() {
    std::cout << "��Ϸ��������" << std::endl;
}

GameScene::~GameScene() {
    std::cout << "��Ϸ��������" << std::endl;
}

void GameScene::OnEnter() {
    std::cout << "������Ϸ����" << std::endl;

    // ���ر���
	// TODO: ���ݴ����Background����ѡ��ͬ����
	background_ = ResourceManager::GetInstance().GetTexture("IMAGE_background_day");
    if (!background_) {
        std::cerr << "�޷�������Ϸ����" << std::endl;
    }

    mBoard = std::make_unique<Board>();

    SetupUI();
}

void GameScene::OnExit() {
    std::cout << "�˳���Ϸ����" << std::endl;
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
    // ���Ʊ���
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
	// TODO: ��Ϸ�ڼ����¼�����
}

void GameScene::SetupUI() {
    /*
    // �������˵���ť
    auto backButton = uiManager_.CreateButton(Vector(700, 30), Vector(80, 30));
    backButton->SetText(u8"����");
    backButton->SetClickCallBack([this]() { OnBackToMenuClicked(); });

    // ���¿�ʼ��ť
    auto restartButton = uiManager_.CreateButton(Vector(700, 80), Vector(80, 30));
    restartButton->SetText(u8"����");
    restartButton->SetClickCallBack([this]() { OnRestartClicked(); });
    */
}

void GameScene::OnBackToMenuClicked() {
    std::cout << "�������˵�" << std::endl;
    /*
    // ������Ϸ���ݣ������Ҫ��
    int score = 100; // ʾ������
    SceneManager::GetInstance().SetGlobalData("last_score", std::to_string(score));

    SceneManager::GetInstance().SwitchTo("MainMenuScene");
    */
}

void GameScene::OnRestartClicked() {
    std::cout << "���¿�ʼ��Ϸ" << std::endl;
    // ���³�ʼ����Ϸ״̬
    mBoard = std::make_unique<Board>();
}