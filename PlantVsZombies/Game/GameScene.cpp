#include "GameScene.h"
#include "CursorObjectManager.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include "./CardSlotManager.h"
#include "./CardComponent.h"
#include "../ResourceKeys.h"
#include "../DeltaTime.h"
#include "./AudioSystem.h"
#include "../GameInfoSaver.h"
#include "GameProgress.h"
#include "ChooseCardUI.h"
#include "../UI/GameMessageBox.h"
#include "../GameApp.h"
#include "../Graphics.h"
#include "../Profiler.h"
#include "./Shovel.h"
#include "ShovelBank.h"
#include "../Logger.h"
#include <cmath>
#include <cstdio>

namespace {
	// 右下角关卡名/轮数显示。
	// 冒险模式：沿用原左对齐（左端锚点 x=768，阴影 766），不改动既有观感。
	// 生存模式：右对齐——右端锚点固定，文字越长越向左延伸，避免"第10面旗"等长文本撞到右侧"难度"文字。
	// 右锚点取 1020：第1轮文本宽约 252px，drawX≈768，与原左对齐位置基本重合，故第1轮观感不变。
	constexpr float kLevelNameRightAnchor = 865.0f;

	void DrawLevelName(GameAPP& gameApp, const std::string& name, bool rightAligned) {
		if (rightAligned) {
			float drawX = kLevelNameRightAnchor;   // 兜底：取不到字体时退化为右端起点
			if (TTF_Font* font = ResourceManager::GetInstance().GetFont(
				ResourceKeys::Fonts::FONT_FZCQ, 21)) {
				int tw = 0, th = 0;
				TTF_SizeUTF8(font, name.c_str(), &tw, &th);
				drawX = kLevelNameRightAnchor - static_cast<float>(tw);
			}
			// 阴影相对主体偏移沿用原 (766,575)/(768,576)：左上 2px / 下 1px
			gameApp.DrawText(name, Vector(drawX - 2.0f, 575.0f), { 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText(name, Vector(drawX, 576.0f), { 223,186,98,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
		}
		else {
			gameApp.DrawText(name, Vector(766, 575), { 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText(name, Vector(768, 576), { 223,186,98,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 21);
		}
	}
}

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
					if (!mBoard || mBoard->mBoardState != BoardState::GAME) return;  // 选卡阶段隐藏
					DrawLevelName(GameAPP::GetInstance(), mBoard->mLevelName, mBoard->mIsSurvival);
				},
				LAYER_UI);

			RegisterDrawCommand("Difficulty",
				[this](Graphics* g) {
					if (!mBoard || mBoard->mBoardState != BoardState::GAME) return;  // 选卡阶段隐藏
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

		// 生存第 1 轮的 committed-pan 退出存档（点完"摇滚"但过场途中退出）里没有小推车：
		// 那份 GAME 快照存盘时 StartGame() 从未运行过，小推车尚未生成。清读档标记，
		// 让下面的 StartGame 正常初始化小推车。判别条件无歧义：第 0 波时小推车不可能被用掉
		// （触发小推车需僵尸、僵尸需波次≥1），故"第1轮+第0波+无小推车" 只可能是 committed-pan。
		// 第 2 轮起小推车从第 1 轮保留、已在存档中，不进此分支（mIsLoadSave 维持 true，绝不重建）。
		if (mBoard->mIsSurvival && mBoard->mSurvivalRound == 1
			&& mBoard->mCurrentWave == 0
			&& mBoard->mEntityManager.GetAllMowerIDs().empty()) {
			mBoard->mIsLoadSave = false;
		}

		mBoard->StartGame();

		AddTexture(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG,
			130.0f, -10.0f, 0.85f, 0.9f, LAYER_UI, true);

		mBoard->mIsLoadSave = false;
	}
	else {
		AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);
		// 生存第 1 轮的轮间存档（首次选卡，StartGame 从未运行过）里没有小推车数据，
		// 需清掉读档标记，让随后选完卡的 StartGame 正常初始化小推车。
		// 第 2 轮起的存档已含小推车（且可能因第 1 轮被用掉而合法地变少甚至为空），
		// 保持 mIsLoadSave=true，由存档恢复，绝不重新生成。
		if (mBoard->mIsSurvival && mBoard->mSurvivalRound == 1) {
			mBoard->mIsLoadSave = false;
		}
	}
}

void GameScene::OnExit() {
	auto& gameApp = GameAPP::GetInstance();
	// 生存模式：玩家已点"一起摇滚吧"进入回移过场（READY_SET_PLANT）、但 mBoardState 要等过场结束
	// 才翻成 GAME 的这 3 秒窗口里退出——此刻卡已提交、本轮在即，逻辑上已"进入游戏"。
	// 按 GAME 持久化，避免重进被误判为"仍在选卡"而重播选卡（卡槽还原committed卡 → 可重复选卡的错乱）。
	// 仅 survival 分支：普通模式过场期 mBoardState 同为 CHOOSE_CARD，但 mIsSurvival=false 不进此分支，行为零改动。
	if (mBoard->mIsSurvival && mBoard->mBoardState == BoardState::CHOOSE_CARD
		&& mCurrentStage == IntroStage::READY_SET_PLANT) {
		mBoard->mBoardState = BoardState::GAME;
	}
	const bool saveState = (mBoard->mBoardState == BoardState::GAME) ||
		(mBoard->mIsSurvival && mBoard->mBoardState == BoardState::CHOOSE_CARD);
	if (saveState && !mReadyToRestart) {
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

	buttons.push_back({ u8"", Vector(455, 250),Vector(42, 39), 1,[]() {
		auto& gameApp = GameAPP::GetInstance();
		gameApp.mShowPlantHP = !gameApp.mShowPlantHP;
	}, ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0 ,false });

	buttons.push_back({ u8"", Vector(590, 250),Vector(42, 39), 1,[]() {
		auto& gameApp = GameAPP::GetInstance();
		gameApp.mShowZombieHP = !gameApp.mShowZombieHP;
	}, ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0 ,false });

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
	texts.push_back
	({ Vector(498, 256), 14, u8"植物血量显示" , glm::vec4{ 107, 109, 144, 255} });
	texts.push_back
	({ Vector(634, 256), 14, u8"僵尸血量显示" , glm::vec4{ 107, 109, 144, 255} });

	mMenu = mUIManager.CreateMessageBox(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f),
		"", buttons, sliders, texts, "", 1.0f, ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK);

	auto& buttonChecks = mMenu.lock()->m_buttons;
	auto& gameApp = GameAPP::GetInstance();
	if (buttonChecks.size() > 4 && buttonChecks[4]) {
		buttonChecks[4]->SetChecked(gameApp.mShowPlantHP);
	}
	if (buttonChecks.size() > 5 && buttonChecks[5]) {
		buttonChecks[5]->SetChecked(gameApp.mShowZombieHP);
	}
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
		{
			PROFILE_SCOPE("2a.Board_Update");
			mBoard->Update();
		}

		// 轮清后存档：BeginSurvivalCardSelect 置位，在 Board::Update 返回后（已脱离 Die() 调用栈）执行。
		// 触发轮清的那只濒死僵尸此刻仍在 EntityManager 中，由 SaveLevelData 内的 IsActive() 过滤排除。
		if (mPendingSurvivalSave) {
			mPendingSurvivalSave = false;
			GameAPP::GetInstance().mGameInfoSaver.SaveLevelData(mBoard.get(), mCardSlotManager);
		}

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
		case IntroStage::FINISH:
			// 入场动画已结束，无需处理
			break;
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
				mSeedbankAdded = true;
			}
			// 选卡界面：首次进入或生存轮间（上一轮选完已销毁）时按需创建
			if (!mChooseCardUI) {
				mChooseCardUI = GameObjectManager::GetInstance().
					CreateGameObjectImmediate<ChooseCardUI>(LAYER_UI, this);
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
				if (mSurvivalRoundTransition) {
					// 生存轮间：铲子/小推车/音乐已存在，不重建，只恢复波次推进
					mSurvivalRoundTransition = false;
					if (mBoard) {
						mBoard->DestroyPreviewZombies();   // 清掉选卡阶段的预览僵尸
						mBoard->mBoardState = BoardState::GAME;
					}
					// 恢复铲子显示
					if (mShovelUI) mShovelUI->SetActive(true);
					if (auto shovel = mBoard->mShovel.lock()) shovel->SetActive(true);
					// 切回战斗音乐（生存固定白天背景；与 Board::StartGame 的 GROUND_DAY 分支一致）
					AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_DAY, -1);
					RegisterSurvivalGameUiOnce();
				}
				else if (mBoard) {
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
			case PromptStage::NONE:
				// 无激活提示，无需处理
				break;
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
				g->LogicalToWorld(142, 42), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 17);
		},
		LAYER_UI + 100000);
	SortDrawCommands();
	mSunCounterRegistered = true;
}

void GameScene::BeginSurvivalCardSelect()
{
	mSurvivalRoundTransition = true;

	// 让上一轮已升起的旗子平滑降回（否则会一直悬着，直到下一轮第 1 波 SetupFlags 才突变重置）。
	// 滑块归位由 GameProgress::Update 依据 mCurrentWave(已被 OnSurvivalRoundClear 归 0)自动完成。
	if (mGameProgress) mGameProgress->LowerAllFlags(1.0f);

	// 清空卡槽前，快照仍在冷却中的卡牌冷却进度，供选完后还原（避免轮末冷却被清空丢失）
	mSurvivalCardCooldowns.clear();
	if (mCardSlotManager) {
		for (auto* card : mCardSlotManager->GetCards()) {
			if (!card) continue;
			auto comp = card->GetComponent<CardComponent>();
			if (comp && comp->IsCooldown()) {
				mSurvivalCardCooldowns[comp->GetPlantType()] =
					{ comp->GetCooldownTimer(), comp->GetCooldownTime() };
			}
		}
	}

	// 清空上一轮的卡槽（空槽重选），场上植物保留
	if (mCardSlotManager)
		mCardSlotManager->ClearAllCards();

	// 轮间选卡：切到选卡背景音乐，进入下一轮时切回战斗音乐（见 READY_SET_PLANT 轮间分支）
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);

	// 选卡阶段隐藏铲子（不可用），进入下一轮时再恢复（见 READY_SET_PLANT 轮间分支）
	if (mShovelUI) mShovelUI->SetActive(false);
	if (auto shovel = mBoard->mShovel.lock()) shovel->SetActive(false);

	// 生成"即将打的那轮"的预览僵尸（mSurvivalRound 已自增、mSpawnZombieList 已重建），
	// 散落在右侧生成区，随相机平移露出；进入下一轮时销毁（见 READY_SET_PLANT 轮间分支）
	mBoard->CreatePreviewZombies();

	// 复用整套开场过场动画：相机平移"回到右边"露出僵尸区 → 选卡UI 滑入（种子槽保持停靠，不再滑落）。
	// 重置各阶段动画计时与一次性标记，使 BACKGROUND_MOVE / SEEDBANK_SLIDE 阶段能重新走一遍。
	// 注意：mSeedbankAdded 保持 true（种子槽纹理已存在，不重复添加）；
	//      mChooseCardUI 当前为 nullptr，SEEDBANK_SLIDE 会据此重新创建选卡界面。
	mAnimElapsed = 0.0f;
	mHasEnter = false;
	// 种子槽轮间不重新滑落：上一轮已停靠在 y=-10，这里把滑落动画直接标记为已完成，
	// SEEDBANK_SLIDE 阶段会令 currentY 恒为 targetY(-10)，种子槽保持停靠、无上下抽动。
	// （首次进入游戏时此值为 0，正常播放滑入动画，不受影响。）
	mSeedbankAnimElapsed = mSeedbankAnimDuration;
	mChooseCardUIAnimElapsed = 0.0f;
	mChooseCardUIMoving = false;
	mReadyAnimElapsed = 0.0f;
	mCurrentStage = IntroStage::BACKGROUND_MOVE;

	// 轮间存档点：延后一帧执行（见 mPendingSurvivalSave）。
	// 本函数由最后一只僵尸的 Die() 中途调用，该僵尸此刻仍在 EntityManager 中、
	// 尚未被 GameObjectManager 清理；若此处同帧存档会把濒死僵尸误序列化进存档。
	mPendingSurvivalSave = true;
}

void GameScene::ChooseCardComplete()
{
	LOG_INFO("GameScene") << "选卡完成，准备开始游戏";
	if (mCurrentStage != IntroStage::COMPLETE) return;

	if (mChooseCardUI) {
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager);
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI = nullptr;
	}

	// 还原轮末快照的冷却：对重新选回的同种植物恢复其冷却进度（map 为空则无操作）
	if (!mSurvivalCardCooldowns.empty() && mCardSlotManager) {
		for (auto* card : mCardSlotManager->GetCards()) {
			if (!card) continue;
			auto comp = card->GetComponent<CardComponent>();
			if (!comp) continue;
			auto it = mSurvivalCardCooldowns.find(comp->GetPlantType());
			if (it != mSurvivalCardCooldowns.end()) {
				comp->RestoreCooldown(it->second.first, it->second.second);
			}
		}
		mSurvivalCardCooldowns.clear();
	}

	// 所有模式统一走相机回移过场（READY_SET_PLANT）。
	// 生存轮间与首次进入的区别仅在该阶段末尾：轮间不重建铲子/小推车/音乐
	// （见 READY_SET_PLANT 对 mSurvivalRoundTransition 的判断）。
	mCurrentStage = IntroStage::READY_SET_PLANT;
	mReadyAnimElapsed = 0.0f;   // 复位，保证回移动画重新播放（轮间为第二次进入此阶段）
	mReadyStartPos = Vector(mCurrectSceneX, 0);

	RegisterSurvivalGameUiOnce();
}

void GameScene::RegisterSurvivalGameUiOnce()
{
	if (mGameUiRegistered) return;
	mGameUiRegistered = true;

	RegisterDrawCommand("ZombieNumber",
		[this](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(3, 569), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
			gameApp.DrawText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(5, 570), { 223,186 ,98 ,255 }, ResourceKeys::Fonts::FONT_FZCQ, 24);
		},
		LAYER_UI);
	RegisterDrawCommand("LevelName",
		[this](Graphics* g) {
			if (!mBoard || mBoard->mBoardState != BoardState::GAME) return;  // 选卡阶段隐藏
			DrawLevelName(GameAPP::GetInstance(), mBoard->mLevelName, mBoard->mIsSurvival);
		},
		LAYER_UI);
	RegisterDrawCommand("Difficulty",
		[this](Graphics* g) {
			if (!mBoard || mBoard->mBoardState != BoardState::GAME) return;  // 选卡阶段隐藏
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1030, 575), { 0,0,0,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
			gameApp.DrawText("难度: " + std::to_string(gameApp.Difficulty),
				Vector(1032, 576), { 223,186,98,255 }, ResourceKeys::Fonts::FONT_FZCQ, 21);
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