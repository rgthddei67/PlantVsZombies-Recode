#include "MainMenuScene.h"
#include "../DeltaTime.h"
#include "AudioSystem.h"
#include "../ResourceKeys.h"
#include "GameObjectManager.h"

void MainMenuScene::OnEnter() 
{
	Scene::OnEnter();
	mGameButton = GameObjectManager::GetInstance().CreateGameObject<GameButton>(LAYER_UI
	, &mUIManager, this);
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_MAINMENU, -1);
}

void MainMenuScene::OnExit() 
{
	GameObjectManager::GetInstance().DestroyGameObject(mGameButton);
	mOpitionButton.reset();
	Scene::OnExit();
}

void MainMenuScene::Update()
{
	Scene::Update();
	if (mReadyToSwitchAdventureLevel) {
		mReadyToSwitchAdventureLevel = false;
		auto& gameApp = GameAPP::GetInstance();
		auto& SceneMgr = SceneManager::GetInstance();

		gameApp.GetGraphics().SetCameraPosition(0, 0);

		SceneMgr.SetGlobalData("EnterLevel", std::to_string(gameApp.mAdventureLevel));
		SceneMgr.SwitchTo("GameScene");
		return;
	}
}

void MainMenuScene::Draw(Graphics* g)
{
	Scene::Draw(g);
	if (!mOpenMenu &&mOpitionButton) {
		mOpitionButton->Draw(g);
	}
}

void MainMenuScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	RegisterDrawCommand("DrawLevel",
		[this](Graphics* g) {
			if (this->mOpenMenu) return;
			auto& gameApp = GameAPP::GetInstance();
			int mBigLevel = gameApp.mAdventureLevel / 10 + 1;
			int mSmallLevel = gameApp.mAdventureLevel % 10;
			gameApp.DrawText(std::to_string(mBigLevel), Vector(703, 158),
				glm::vec4{ 255, 255, 255, 255 });
			gameApp.DrawText(std::to_string(mSmallLevel), Vector(722, 160),
				glm::vec4{ 255, 255, 255, 255 });
		},
		LAYER_UI + 10000);
	SortDrawCommands();

	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_BG, 0.0f, 0.0f, 12.0f, 12.0f, -10);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_BG_CENTER, 80.0f, 300.0f, 1.0f, 1.0f, 0);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_BG_LEFT, 0.0f, 0.0f, 1.0f, 1.0f, 3);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_BG_RIGHT, 210.0f, 40.0f, 1.0f, 1.0f, 5);
	// 花瓶
	mOpitionButton = mUIManager.CreateButton(Vector(704, 485), Vector(48 * 1.5f, 22 * 1.5f));
	mOpitionButton->SetAsCheckbox(false);
	mOpitionButton->SetSkipDraw(true);
	mOpitionButton->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_OPTIONS1,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_OPTIONS2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_OPTIONS2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_OPTIONS2);
	mOpitionButton->SetClickCallBack([this](bool) {
		this->OpenMenu();
		});
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_HELP1, 786.0f, 515.0f, 1.0f, 1.0f, 10);
	auto button = mUIManager.CreateButton(Vector(855, 495), Vector(47 * 1.2f, 27 * 1.2f));
	mExitButton = button;
	button->SetAsCheckbox(false);
	button->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT1,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2);
	button->SetClickCallBack([this](bool) {
		GameAPP::GetInstance().SetRunning(false);
		});
	// 花
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER1, 825.0f, 420.0f, 1.0f, 1.0f, 12);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER2, 785.0f, 439.0f, 1.0f, 1.0f, 12);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER3, 870.0f, 450.0f, 1.0f, 1.0f, 12);
}

void MainMenuScene::OpenMenu() 
{
	if (mOpenMenu) return;

	mOpenMenu = true;
	DeltaTime::SetPaused(true);
	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig> texts;
	buttons.push_back({ u8"返回游戏", Vector(400, 430),Vector(360, 100), 40,[this]() {
		mOpenMenu = false;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_OPTIONS_BACKTOGAMEBUTTON0 ,true });

	sliders.push_back({ Vector(530, 175), Vector(135, 10),
		0.0f ,1.0f, AudioSystem::GetMusicVolume(),[](float value) {
		AudioSystem::SetMusicVolume(value);
	} });

	sliders.push_back({ Vector(530, 200), Vector(135, 10),
		0.0f ,1.0f, AudioSystem::GetSoundVolume(),[](float value) {
		AudioSystem::SetSoundVolume(value);
	} });

	sliders.push_back({ Vector(530, 225), Vector(135, 10),
		1, 7, static_cast<float>(GameAPP::GetInstance().Difficulty), [](float value) {
		GameAPP::GetInstance().Difficulty = static_cast<int>(value);
	} });

	texts.push_back
	({ Vector(480, 165), 22, u8"音乐" , glm::vec4{ 107, 109, 144, 255} });
	texts.push_back
	({ Vector(480, 190), 22, u8"音效" , glm::vec4{ 107, 109, 144, 255} });
	texts.push_back
	({ Vector(480, 215), 22, u8"难度" , glm::vec4{ 107, 109, 144, 255} });

	mMenu = mUIManager.CreateMessageBox(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f),
		"", buttons, sliders, texts, "", 1.0f, ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK);
}