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
    AddTexture("IMAGE_background_day", -215, 0, 1.0f, 1.0f, -10000);    
    AddTexture("IMAGE_SeedBank_Long", 5.0f, -10.0f, 0.85f, 0.9f, 10000);

    mBoard = std::make_unique<Board>();
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

void GameScene::Draw(SDL_Renderer* renderer) {
    Scene::Draw(renderer);

    // ������Ϸ����
    if (mBoard) {
        mBoard->Draw(renderer);
    }

    // ����UI
    uiManager_.DrawAll(renderer);
}

void GameScene::HandleEvent(SDL_Event& event, InputHandler& input) 
{
    Scene::HandleEvent(event, input);
	// TODO: ��Ϸ�ڼ����¼�����
}

/*
void GameScene::SetupUI() {
    // �������˵���ť
    auto backButton = uiManager_.CreateButton(Vector(700, 30), Vector(80, 30));
    backButton->SetText(u8"����");
    backButton->SetClickCallBack([this]() { OnBackToMenuClicked(); });

    // ���¿�ʼ��ť
    auto restartButton = uiManager_.CreateButton(Vector(700, 80), Vector(80, 30));
    restartButton->SetText(u8"����");
    restartButton->SetClickCallBack([this]() { OnRestartClicked(); });
}
*/
void GameScene::OnBackToMenuClicked() {
    std::cout << "�������˵�" << std::endl;
    /*
    // ������Ϸ���ݣ������Ҫ��
    int score = 100;
    SceneManager::GetInstance().SetGlobalData("last_score", std::to_string(score));

    SceneManager::GetInstance().SwitchTo("MainMenuScene");
    */
}

void GameScene::OnRestartClicked() {
    std::cout << "���¿�ʼ��Ϸ" << std::endl;
    // ���³�ʼ����Ϸ״̬
    mBoard = std::make_unique<Board>();
}