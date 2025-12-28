#include "GameScene.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include "./Plant/PlantType.h"
#include "./CardSlotManager.h"
#include <iostream>

GameScene::GameScene() {
	std::cout << "游戏场景创建" << std::endl;
}

GameScene::~GameScene() {
	std::cout << "游戏场景销毁" << std::endl;
}

void GameScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	// TODO: 根据传入的Background参数选择不同背景
	AddTexture("IMAGE_background_day", -215, 0, 1.0f, 1.0f, LAYER_BACKGROUND);
	AddTexture("IMAGE_SeedBank_Long", 5, -10, 0.85f, 0.9f, LAYER_UI);

	if (mBoard) {
		RegisterDrawCommand("Board",
			[this](SDL_Renderer* renderer) { mBoard->Draw(renderer); },
			LAYER_GAME_OBJECT);
		RegisterDrawCommand("SunCounter",
			[this](SDL_Renderer* renderer) {
				GameAPP::GetInstance().DrawText(renderer, std::to_string(mBoard->GetSun()),
					Vector(20, 43), SDL_Color{ 0, 0, 0, 255 }, "./font/fzcq.ttf", 17);
			},
			LAYER_UI + 100);
	}
	SortDrawCommands();
}

void GameScene::OnEnter() {
	Scene::OnEnter();
	std::cout << "进入游戏场景" << std::endl;

	// 加载背景
	mBoard = std::make_unique<Board>();

	auto CardUI = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameObject>();
	CardUI->SetName("CardUI");
	auto cardSlotManager = CardUI->AddComponent<CardSlotManager>(mBoard.get());

	cardSlotManager->AddCard(PlantType::PLANT_SUNFLOWER, 50, 7.5f);

	std::cout << "Card system initialized with " << cardSlotManager->GetCards().size() << " cards" << std::endl;
}

void GameScene::OnExit() {
	Scene::OnExit();
	std::cout << "退出GameScene" << std::endl;
	mBoard.reset();
}

void GameScene::Update() {
	Scene::Update();
	if (mBoard)
	{
		mBoard->Update();
	}
}

void GameScene::HandleEvent(SDL_Event& event, InputHandler& input)
{
	Scene::HandleEvent(event, input);
	mUIManager.UpdateAll(&input);
	// TODO: 游戏内键盘事件处理
}

/*
void GameScene::SetupUI() {
	// 返回主菜单按钮
	auto backButton = uiManager_.CreateButton(Vector(700, 30), Vector(80, 30));
	backButton->SetText(u8"返回");
	backButton->SetClickCallBack([this]() { OnBackToMenuClicked(); });

	// 重新开始按钮
	auto restartButton = uiManager_.CreateButton(Vector(700, 80), Vector(80, 30));
	restartButton->SetText(u8"重玩");
	restartButton->SetClickCallBack([this]() { OnRestartClicked(); });
}
*/
void GameScene::OnBackToMenuClicked() {
	std::cout << "返回主菜单" << std::endl;
	/*
	// 保存游戏数据（如果需要）
	int score = 100;
	SceneManager::GetInstance().SetGlobalData("last_score", std::to_string(score));

	SceneManager::GetInstance().SwitchTo("MainMenuScene");
	*/
}

void GameScene::OnRestartClicked() {
	std::cout << "重新开始游戏" << std::endl;
	// 重新初始化游戏状态
	mBoard = std::make_unique<Board>();
}