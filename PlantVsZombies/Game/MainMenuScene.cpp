#include "MainMenuScene.h"
#include "../DeltaTime.h"
#include "AudioSystem.h"
#include "../ResourceKeys.h"
#include "GameObjectManager.h"
#include "../UI/GameMessageBox.h"
#include "Board.h"

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
	mGameButton = nullptr;
	mOpitionButton.reset();
	mAlmanacButton.reset();
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
	if (mReadyToSwitchAlmanac) {
		mReadyToSwitchAlmanac = false;
		auto& gameApp = GameAPP::GetInstance();
		auto& SceneMgr = SceneManager::GetInstance();
		gameApp.GetGraphics().SetCameraPosition(0, 0);
		SceneMgr.SwitchTo("AlmanacScene");
	}
	if (mReadyToSwitchSurvival) {
		mReadyToSwitchSurvival = false;
		auto& gameApp = GameAPP::GetInstance();
		auto& SceneMgr = SceneManager::GetInstance();
		gameApp.GetGraphics().SetCameraPosition(0, 0);
		// 不再直进无尽关，先进「选择关卡」界面，由玩家选白天/黑夜无尽
		SceneMgr.SwitchTo("GameSelectScene");
		return;
	}
}

void MainMenuScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	RegisterDrawCommand("DrawLevel",
		[this](Graphics* g) {
			if (this->mOpenMenu) return;
			auto& gameApp = GameAPP::GetInstance();
			int mBigLevel = (gameApp.mAdventureLevel - 1) / 9 + 1;
			int mSmallLevel = (gameApp.mAdventureLevel - 1) % 9 + 1;
			gameApp.DrawText(std::to_string(mBigLevel), Vector(703, 158),
				glm::vec4(255.0f, 255.0f, 255.0f, 255.0f));
			gameApp.DrawText(std::to_string(mSmallLevel), Vector(722, 160),
				glm::vec4(255.0f, 255.0f, 255.0f, 255.0f));
		},
		LAYER_UI + 10000);
	RegisterDrawCommand("DrawButton",
		[this](Graphics* g) {
			if (!mOpenMenu && mOpitionButton) {
				mOpitionButton->Draw(g);
			}
			if (!mOpenMenu && mAlmanacButton) {
				mAlmanacButton->Draw(g);
			}
		},
		LAYER_UI + 100);

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
	mExitButton = mUIManager.CreateButton(Vector(855, 495), Vector(47 * 1.2f, 27 * 1.2f));
	mExitButton->SetAsCheckbox(false);
	mExitButton->SetImageKeys(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT1,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2,
		ResourceKeys::Textures::IMAGE_SELECTORSCREEN_QUIT2);
	mExitButton->SetClickCallBack([](bool) {
		GameAPP::GetInstance().SetRunning(false);
		});
	// 花
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER1, 825.0f, 420.0f, 1.0f, 1.0f, 12);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER2, 785.0f, 439.0f, 1.0f, 1.0f, 12);
	AddTexture(ResourceKeys::Textures::IMAGE_SELECTORSCREEN_FLOWER3, 870.0f, 450.0f, 1.0f, 1.0f, 12);

	mAlmanacButton = mUIManager.CreateButton(Vector(521, 441), Vector(99 * 1.0f, 99 * 1.0f));
	mAlmanacButton->SetAsCheckbox(false);
	mAlmanacButton->SetSkipDraw(true);
	mAlmanacButton->SetImageKeys("IMAGE_SELECTORSCREEN_ALMANAC",
		"IMAGE_SELECTORSCREEN_ALMANAC",
		"IMAGE_SELECTORSCREEN_ALMANAC",
		"IMAGE_SELECTORSCREEN_ALMANAC");
	mAlmanacButton->SetClickCallBack([this](bool) {
		this->mReadyToSwitchAlmanac = true;
		});
}

void MainMenuScene::OpenMenu()
{
	if (mOpenMenu) return;

	mOpenMenu = true;
	DeltaTime::SetPaused(true);
	if (mAlmanacButton) mAlmanacButton->SetEnabled(false);
	if (mOpitionButton) mOpitionButton->SetEnabled(false);
	if (mGameButton) mGameButton->SetEnabled(false);
	auto& gameApp = GameAPP::GetInstance();
	const glm::vec4 labelColor{ 107, 109, 144, 255 };
	// 四个复选框初始态来自各自不同的状态变量（mVsync / IsFullscreen / mShowPlantHP /
	// mShowZombieHP），Builder 写法按项绑定 initChecked，原"按槽位赋值错位"bug 类别不复存在
	mMenu = GameMessageBox::Builder(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f))
		.Background(ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK)
		.Button(u8"返回游戏", Vector(400, 430), Vector(360, 100), 40, [this]() {
			if (mAlmanacButton) mAlmanacButton->SetEnabled(true);
			if (mOpitionButton) mOpitionButton->SetEnabled(true);
			if (mGameButton) mGameButton->SetEnabled(true);
			mOpenMenu = false;
			DeltaTime::SetPaused(false);
		}, ResourceKeys::Textures::IMAGE_OPTIONS_BACKTOGAMEBUTTON0)
		.Checkbox(Vector(510, 250), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.ApplyVsync(!app.mVsync);
		}, gameApp.mVsync)
		.Checkbox(Vector(510, 290), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.SetFullscreen(!app.IsFullscreen());
		}, gameApp.IsFullscreen())
		.Checkbox(Vector(510, 330), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowPlantHP = !app.mShowPlantHP;
		}, gameApp.mShowPlantHP)
		.Checkbox(Vector(510, 370), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowZombieHP = !app.mShowZombieHP;
		}, gameApp.mShowZombieHP)
		.Slider(Vector(530, 175), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetMusicVolume(),
			[](float v) { AudioSystem::SetMusicVolume(v); })
		.Slider(Vector(530, 200), Vector(135, 10), 0.0f, 1.0f, AudioSystem::GetSoundVolume(),
			[](float v) { AudioSystem::SetSoundVolume(v); })
		.Slider(Vector(530, 225), Vector(135, 10), 1, 7,
			static_cast<float>(GameAPP::GetInstance().Difficulty),
			[](float v) { GameAPP::GetInstance().Difficulty = static_cast<int>(v); }, true)
		.Text(Vector(480, 165), 22, u8"音乐", labelColor)
		.Text(Vector(480, 190), 22, u8"音效", labelColor)
		.Text(Vector(480, 215), 22, u8"难度", labelColor)
		.Text(Vector(555, 254), 18, u8"垂直同步", labelColor)
		.Text(Vector(555, 294), 18, u8"全屏", labelColor)
		.Text(Vector(555, 334), 18, u8"植物血量显示", labelColor)
		.Text(Vector(555, 374), 18, u8"僵尸血量显示", labelColor)
		.Show();
}