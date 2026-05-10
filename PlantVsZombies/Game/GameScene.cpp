#include "GameScene.h"
#include "CursorObjectManager.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include "./CardSlotManager.h"
#include "../ResourceKeys.h"
#include "../DeltaTime.h"
#include "./AudioSystem.h"
#include "../GameInfoSaver.h"
#include "GameProgress.h"
#include "ChooseCardUI.h"
#include "../UI/GameMessageBox.h"
#include "../GameApp.h" 
#include "../Graphics.h"
#include "./Shovel.h"
#include "ShovelBank.h"
#include <iostream>
#include <cmath>
#include <cstdio>

GameScene::GameScene() {

}

GameScene::~GameScene() {

}

void GameScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();

	Background background = GameAPP::GetInstance().GetBackgroundID
		(std::stoi(SceneManager::GetInstance().GetGlobalData("EnterLevel")));

	if (background == Background::GROUND_DAY) {
		AddTexture(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY,
			mStartX, mBackgroundY, 1.0f, 1.0f, LAYER_BACKGROUND, false);
	}
	else if (background == Background::GROUND_NIGHT) {
		AddTexture(ResourceKeys::Textures::IMAGE_BACKGROUND_NIGHT,
			mStartX, mBackgroundY, 1.0f, 1.0f, LAYER_BACKGROUND, false);
	}

	if (mBoard) {
		RegisterDrawCommand("Prompt",
			[this](Graphics* g) {
				if (!mPrompt.active) return;

				auto texture = ResourceManager::GetInstance().GetTexture(mPrompt.textureKey);
				if (!texture) return;

				float w = static_cast<float>(texture->width) * mPrompt.scale;
				float h = static_cast<float>(texture->height) * mPrompt.scale;
				float centerX = SCENE_WIDTH / 2.0f;
				float centerY = SCENE_HEIGHT / 2.0f;
				float drawX = centerX - w / 2;
				float drawY = centerY - h / 2;

				glm::vec4 tint(255.0f, 255.0f, 255.0f, mPrompt.alpha);
				g->DrawTexture(texture, drawX, drawY, w, h, 0.0f, tint);
			},
			LAYER_UI + 1000);

		// 读档恢复时，board 已处于 GAME 状态，需在此重新注册 UI 文字命令
		if (mBoard->mBoardState == BoardState::GAME) {
			RegisterDrawCommand("ZombieNumber",
				[this](Graphics* g) {
					auto& gameApp = GameAPP::GetInstance();
					gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
						Vector(3, 569), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
					gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
						Vector(5, 570), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
				},
				LAYER_UI);

			RegisterDrawCommand("LevelName",
				[this](Graphics* g) {
					auto& gameApp = GameAPP::GetInstance();
					gameApp.DrawText(mBoard->mLevelName,
						Vector(766, 575), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
					gameApp.DrawText(mBoard->mLevelName,
						Vector(768, 576), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
				},
				LAYER_UI);

			RegisterDrawCommand("Difficulty",
				[](Graphics* g) {
					auto& gameApp = GameAPP::GetInstance();
					gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
						Vector(1030, 575), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
					gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
						Vector(1032, 576), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
				},
				LAYER_UI);
			ShowSunCount();
		}
	}
}

void GameScene::OnEnter() {
	Scene::OnEnter();

	int enterLevel = std::stoi(SceneManager::GetInstance().GetGlobalData("EnterLevel"));

	mBoard = std::make_unique<Board>(this, GameAPP::GetInstance().GetBackgroundID(enterLevel), enterLevel);
	auto CardUI = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameObject>(
		LAYER_UI);
	CardUI->SetName("CardUI");
	mCardSlotManager = CardUI->AddComponent<CardSlotManager>(mBoard.get());

	mGameProgress = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameProgress>(
		LAYER_UI, mBoard.get(), this);
	mGameProgress->SetActive(false);

	auto button = mUIManager.CreateButton(Vector(990, -5), Vector(125 * 0.9f, 52 * 0.9f));
	mMainMenuButton = button;
	button->SetText("主菜单");
	button->SetAsCheckbox(false);
	button->SetTextColor(glm::vec4{ 53, 191, 61, 255 });
	button->SetHoverTextColor(glm::vec4{ 53, 240, 61, 255 });
	button->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);
	button->SetClickCallBack([this](bool) {
		this->OpenMenu();
		});

	auto button2 = mUIManager.CreateButton(Vector(990, 45), Vector(125 * 0.9f, 52 * 0.9f));
	mSpeedSettingsButton = button2;
	auto formatSpeedText = [](float scale) {
		char buf[16];
		std::snprintf(buf, sizeof(buf), "x%.1f", scale);
		return std::string(buf);
	};
	button2->SetText(formatSpeedText(DeltaTime::GetTimeScale()));
	button2->SetAsCheckbox(false);
	button2->SetTextColor(glm::vec4{ 53, 191, 61, 255 });
	button2->SetHoverTextColor(glm::vec4{ 53, 240, 61, 255 });
	button2->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
		ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);
	button2->SetClickCallBack([this, formatSpeedText](bool) {
		// 暂停时不响应点击，避免 SetTimeScale 把游戏从暂停状态拉回
		if (DeltaTime::IsPaused()) return;
		float current = DeltaTime::GetTimeScale();
		float next = 1.0f;
		if (current == 1.0f)      next = 2.0f;
		else if (current == 2.0f) next = 0.5f;
		else                       next = 1.0f;
		DeltaTime::SetTimeScale(next);
		if (auto btn = mSpeedSettingsButton.lock()) {
			btn->SetText(formatSpeedText(next));
		}
		});

	// 读档
	GameAPP::GetInstance().mGameInfoSaver.LoadLevelData(mBoard.get(), mCardSlotManager);

	if (mBoard->mBoardState == BoardState::GAME) {
		// 跳过选卡和开场动画，直接进入游戏
		GameAPP::GetInstance().GetGraphics().SetCameraPosition(0, 0);
		mCurrentStage = IntroStage::FINISH;
		mCurrectSceneX = mGameStartX;
		mSeedbankAdded = true;
		mBoard->StartGame();

		AddTexture(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG,
			130.0f, -10.0f, 0.85f, 0.9f, LAYER_UI, true);

		mBoard->mIsLoadSave = false;
	}
	else {
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);
	}
}

void GameScene::OnExit() {
	auto& gameApp = GameAPP::GetInstance();
	if (mBoard->mBoardState == BoardState::GAME && !mReadyToRestart) {
		gameApp.mGameInfoSaver.SaveLevelData
		(mBoard.get(), mCardSlotManager);
	}
	gameApp.mGameInfoSaver.SavePlayerInfo();

	Scene::OnExit();
	mShovelUI = nullptr;
	mBoard.reset();
	mSpeedSettingsButton.reset();
	mMainMenuButton.reset();
	mGameProgress = nullptr;
	mCardSlotManager = nullptr;
	mChooseCardUI = nullptr;
}

void GameScene::OpenMenu()
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
	buttons.push_back({ u8"重新开始", Vector(485, 330),Vector(213 * 0.9f, 50 * 0.9f),
		21 ,[this]() {
		this->OpenRestartMenu();
	}, ResourceKeys::Textures::IMAGE_BUTTONBIG, true });	// 有能力了再改为false

	buttons.push_back({ u8"主菜单", Vector(485, 371),Vector(213 * 0.9f, 50 * 0.9f),
		21 ,[this]() {
		this->OpenQuitMenu();
	}, ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

	buttons.push_back({ u8"查看图鉴", Vector(485, 289),Vector(213 * 0.9f, 50 * 0.9f),
	21 ,[this]() {
		DeltaTime::SetPaused(false);
		this->mLendToAlmanacScene = true;
	}, ResourceKeys::Textures::IMAGE_BUTTONBIG, true });

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

void GameScene::OpenRestartMenu()
{
	if (this->mOpenRestartMenu) return;
	this->mOpenRestartMenu = true;
	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig> texts;

	buttons.push_back({ u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mOpenMenu = false;
		this->mOpenRestartMenu = false;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mReadyToRestart = true;
		this->mOpenRestartMenu = false;
		this->mOpenMenu = false;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	mUIManager.CreateMessageBox(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2),
		u8"    确定重新开始游戏吗?", buttons, sliders, texts, "", 1.5f);
}

void GameScene::OpenQuitMenu()
{
	if (this->mOpenQuitMenu) return;
	this->mOpenQuitMenu = true;

	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig> texts;

	buttons.push_back({ u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mOpenMenu = false;
		this->mOpenQuitMenu = false;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mReadyToBackMenu = true;
		this->mOpenQuitMenu = false;
		this->mOpenMenu = false;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	mUIManager.CreateMessageBox(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2),
		u8"    确定退出这把游戏吗?", buttons, sliders, texts, "", 1.5f);
}

void GameScene::Update() {
	Scene::Update();

	// 同步速度按钮文字与当前时间缩放（仅在非暂停时；暂停期间 timeScale=0，保持上一次显示）
	// 这样 F3 切换 5x、菜单暂停后恢复用户速度，按钮文字都能保持一致
	if (!DeltaTime::IsPaused()) {
		if (auto btn = mSpeedSettingsButton.lock()) {
			char buf[16];
			std::snprintf(buf, sizeof(buf), "x%.1f", DeltaTime::GetTimeScale());
			btn->SetText(buf);
		}
	}

	if (mBoard && !mReadyToRestart && !mReadyToBackMenu)
	{
		mBoard->Update();

		auto& input = GameAPP::GetInstance().GetInputHandler();
		if (mBoard->mBoardState != BoardState::LOSE_GAME && !this->mOpenRestartMenu && (input.IsKeyPressed(SDLK_SPACE) || input.IsKeyPressed(SDLK_ESCAPE))) {
			if (this->mOpenMenu) {
				mOpenMenu = false;
				DeltaTime::SetPaused(false);
				GameObjectManager::GetInstance().DestroyGameObject(mMenu.lock());
				mMenu.reset();
			}
			else {
				this->OpenMenu();
			}
		}

		float deltaTime = DeltaTime::GetDeltaTime();

		switch (mCurrentStage) {
		case IntroStage::BACKGROUND_MOVE:
		{
			if (mBoard->mBoardState != BoardState::CHOOSE_CARD) break;

			if (!mHasEnter) {
				mAnimElapsed += deltaTime;
				if (mAnimElapsed >= mAnimDuration) {
					mAnimElapsed = mAnimDuration;
					mCurrentStage = IntroStage::SEEDBANK_SLIDE;
					mHasEnter = true;
				}
			}

			float t = mAnimElapsed / mAnimDuration;
			float eased = static_cast<float>((1 - cos(t * M_PI)) / 2);

			// 计算背景应有的屏幕坐标
			float screenX = mStartX + (mTargetSceneX - mStartX) * eased;

			// 背景世界坐标固定为 mStartX
			float worldX = mStartX;

			// 摄像机位置 = 世界坐标 - 屏幕坐标
			float camX = worldX - screenX;

			// 移动摄像机（保持 Y 轴不变）
			GameAPP::GetInstance().GetGraphics().SetCameraPosition(camX, 0);

			// 更新 mCurrectSceneX 供后续使用
			mCurrectSceneX = screenX;

			break;
		}
		case IntroStage::SEEDBANK_SLIDE:
		{
			// 首次进入时添加种子槽纹理（初始位置在屏幕外上方）
			if (!mSeedbankAdded) {
				AddTexture(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG,
					130.0f, -100.0f,            // 起始位置：x=130, y 上方
					0.85f, 0.9f, LAYER_UI, true);
				mChooseCardUI = GameObjectManager::GetInstance().
					CreateGameObjectImmediate<ChooseCardUI>(LAYER_UI, this);
				mSeedbankAdded = true;
			}

			// 种子槽滑落动画
			if (mSeedbankAnimElapsed < mSeedbankAnimDuration) {
				mSeedbankAnimElapsed += deltaTime;
				if (mSeedbankAnimElapsed > mSeedbankAnimDuration) mSeedbankAnimElapsed = mSeedbankAnimDuration;
			}

			float t = mSeedbankAnimElapsed / mSeedbankAnimDuration;
			float eased = static_cast<float>((1 - cos(t * M_PI)) / 2);

			float startY = -100.0f, targetY = -10.0f;

			float currentY = startY + (targetY - startY) * eased;
			SetTexturePosition(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG, 130.0f, currentY);

			// --- 选卡界面动画 ---
			if (!mChooseCardUIMoving) {
				mChooseCardUIMoving = true;
				mChooseCardUIAnimElapsed = 0.0f;
			}
			if (mChooseCardUIMoving) {
				mChooseCardUIAnimElapsed += deltaTime;
				if (mChooseCardUIAnimElapsed > mChooseCardUIAnimDuration) {
					mChooseCardUIAnimElapsed = mChooseCardUIAnimDuration;
				}

				float t2 = mChooseCardUIAnimElapsed / mChooseCardUIAnimDuration;
				float eased2 = static_cast<float>((1 - cos(t2 * M_PI)) / 2);
				Vector currentPos = Vector::lerp(mChooseCardUIStartPos, mChooseCardUITargetPos, eased2);
				if (auto transform = mChooseCardUI->GetComponent<TransformComponent>()) {
					transform->SetPosition(currentPos);
				}
			}

			// 检查两个动画是否都完成
			bool seedbankDone = (mSeedbankAnimElapsed >= mSeedbankAnimDuration);
			bool chooseUIDone = (mChooseCardUIAnimElapsed >= mChooseCardUIAnimDuration);
			if (seedbankDone && chooseUIDone) {
				// 确保最终位置准确
				if (auto transform = mChooseCardUI->GetComponent<TransformComponent>()) {
					transform->SetPosition(mChooseCardUITargetPos);
				}
				mChooseCardUI->AddAllCard();
				// 启用按钮
				if (mChooseCardUI && mChooseCardUI->GetButton()) {
					mChooseCardUI->GetButton()->SetEnabled(true);
				}
				mCurrentStage = IntroStage::COMPLETE;
			}
			break;
		}
		case IntroStage::COMPLETE:
		{
			ShowSunCount();
			break;
		}
		case IntroStage::READY_SET_PLANT:
		{
			if (mReadyAnimElapsed < mReadyAnimDuration) {
				mReadyAnimElapsed += deltaTime;
				if (mReadyAnimElapsed > mReadyAnimDuration)
					mReadyAnimElapsed = mReadyAnimDuration;
			}

			float t = mReadyAnimElapsed / mReadyAnimDuration;
			float eased = static_cast<float>((1 - cos(t * M_PI)) / 2);

			// 计算目标屏幕坐标（从当前屏幕坐标移动到 mGameStartX）
			float startScreenX = mTargetSceneX;   // 当前屏幕坐标（由之前阶段更新）
			float targetScreenX = mGameStartX;     // 目标屏幕坐标（-250）

			float screenX = startScreenX + (targetScreenX - startScreenX) * eased;

			float worldX = mStartX;

			float camX = worldX - screenX;

			GameAPP::GetInstance().GetGraphics().SetCameraPosition(camX, 0);

			mCurrectSceneX = screenX;

			if (mReadyAnimElapsed >= mReadyAnimDuration) {
				if (mBoard) {
					mBoard->StartGame();
				}
				mCurrentStage = IntroStage::FINISH;
			}
			break;
		}
		}
		// 处理提示动画
		if (mPrompt.active)
		{
			float delta = DeltaTime::GetDeltaTime();
			mPrompt.timer += delta;

			switch (mPrompt.stage)
			{
			case PromptStage::APPEAR:
			{
				float t = std::min(mPrompt.timer / mPrompt.appearDuration, 1.0f);
				mPrompt.scale = 1.5f - 0.5f * t;          // 1.5 → 1.0
				mPrompt.alpha = static_cast<Uint8>(255 * t); // 0 → 255
				if (mPrompt.timer >= mPrompt.appearDuration)
				{
					mPrompt.stage = PromptStage::HOLD;
					mPrompt.timer = 0.0f;
				}
				break;
			}
			case PromptStage::HOLD:
			{
				if (mPrompt.timer >= mPrompt.holdDuration)
				{
					mPrompt.stage = PromptStage::FADE_OUT;
					mPrompt.timer = 0.0f;
				}
				break;
			}
			case PromptStage::FADE_OUT:
			{
				float t = std::min(mPrompt.timer / mPrompt.fadeDuration, 1.0f);
				mPrompt.alpha = static_cast<Uint8>(255 * (1.0f - t));
				mPrompt.scale = 1.0f + 0.2f * t;       // 放大
				if (mPrompt.timer >= mPrompt.fadeDuration)
				{
					mPrompt.active = false;
					mPrompt.stage = PromptStage::NONE;
				}
				break;
			}
			}
		}

	}

	if (mReadyToRestart) {
		auto& gameApp = GameAPP::GetInstance();
		gameApp.GetGraphics().SetCameraPosition(0, 0);
		gameApp.mGameInfoSaver.DeleteLevelData(mBoard.get());
		SceneManager::GetInstance().SwitchTo("GameScene");
		return;	// 避免场景已经弹出，变量错乱，执行后续代码
	}

	if (mReadyToBackMenu) {
		GameAPP::GetInstance().GetGraphics().SetCameraPosition(0, 0);
		SceneManager::GetInstance().SwitchTo("MainMenuScene");
		return;
	}

	if (mLendToAlmanacScene) {
		GameAPP::GetInstance().GetGraphics().SetCameraPosition(0, 0);
		SceneManager::GetInstance().SwitchTo("AlmanacScene");
		return;
	}
}

void GameScene::ShowSunCount()
{
	if (mSunCounterRegistered) return;
	RegisterDrawCommand("SunCounter",
		[this](Graphics* g) {
			GameAPP::GetInstance().DrawText(std::to_string(mBoard->GetSun()),
				g->ScreenToWorldPosition(142, 42), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 17);
		},
		LAYER_UI + 100000);
	SortDrawCommands();
	mSunCounterRegistered = true;
}

void GameScene::ChooseCardComplete()
{
#ifdef _DEBUG
	std::cout << "选卡完成，准备开始游戏" << std::endl;
#endif
	if (mCurrentStage != IntroStage::COMPLETE) return;
	mCurrentStage = IntroStage::READY_SET_PLANT;
	mReadyStartPos = Vector(mCurrectSceneX, 0);

	if (mChooseCardUI) {
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager);
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI = nullptr;
	}

	RegisterDrawCommand("ZombieNumber",
		[this](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(3, 569), { 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 24);
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(5, 570), { 223,186 ,98 ,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 24);
		},
		LAYER_UI);
	RegisterDrawCommand("LevelName",
		[this](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText(mBoard->mLevelName,
				Vector(766, 575), { 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText(mBoard->mLevelName,
				Vector(768, 576), { 223,186,98,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
		},
		LAYER_UI);
	RegisterDrawCommand("Difficulty",
		[](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1030, 575), { 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1032, 576), { 223,186,98,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
		},
		LAYER_UI);
	SortDrawCommands();
}

void GameScene::ShowShovel()
{
	const Vector shovelBankCenter(850.0f, 30.0f);

	// 先创建铲子背景（renderOrder 较低，画在下面）
	auto shovelBank = GameObjectManager::GetInstance()
		.CreateGameObjectImmediate<ShovelBank>(LAYER_UI, mBoard.get());
	mShovelUI = shovelBank;

	// 再创建铲子（renderOrder 较高，画在上面）
	auto shovelWeak = mBoard->CreateShovel();
	if (auto shovel = shovelWeak.lock())
		shovel->SetHomePosition(shovelBankCenter);
}

GameProgress* GameScene::GetGameProgress() const
{
	return this->mGameProgress;
}

void GameScene::GameOver()
{
	if (mBoard->mBoardState == BoardState::LOSE_GAME) return;

	mBoard->mCursorObjectManager.ClearActive();
	GameAPP::GetInstance().mGameInfoSaver.DeleteLevelData(mBoard.get());
	mUIManager.RemoveButton(this->mMainMenuButton.lock());
	mMainMenuButton.reset();
	if (mShovelUI)
		GameObjectManager::GetInstance().DestroyGameObject(mShovelUI);

	mShovelUI = nullptr;

	if (auto shovel = mBoard->mShovel.lock())
	{
		shovel->Die();
	}

	std::vector<GameMessageBox::ButtonConfig> buttons;
	std::vector<GameMessageBox::SliderConfig> sliders;
	std::vector<GameMessageBox::TextConfig> texts;

	buttons.push_back({ u8"返回菜单", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mReadyToBackMenu = true;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });
	buttons.push_back({ u8"重新开始", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f),14, [this]() {
		this->mReadyToRestart = true;
		DeltaTime::SetPaused(false);
	}, ResourceKeys::Textures::IMAGE_BUTTONSMALL, true });

	mUIManager.CreateMessageBox(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2),
		u8"僵尸吃掉了你的脑子！", buttons, sliders, texts, u8"游戏结束", 1.5f);
}

void GameScene::ShowPrompt(const std::string& textureKey,
	float appearDur,
	float holdDur,
	float fadeDur)
{
	// 如果已有动画正在播放 直接覆盖
	mPrompt.active = true;
	mPrompt.stage = PromptStage::APPEAR;
	mPrompt.timer = 0.0f;
	mPrompt.scale = 1.5f;               // 起始放大
	mPrompt.alpha = 0;                   // 起始透明
	mPrompt.textureKey = textureKey;
	mPrompt.appearDuration = appearDur;
	mPrompt.holdDuration = holdDur;
	mPrompt.fadeDuration = fadeDur;
}