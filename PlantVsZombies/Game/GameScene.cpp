#include "GameScene.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include "./Plant/PlantType.h"
#include "./CardSlotManager.h"
#include "../ResourceKeys.h"
#include "../DeltaTime.h"
#include "./AudioSystem.h"
#include "GameProgress.h"
#include "ChooseCardUI.h"
#include <iostream>

#include "./Zombie/Zombie.h"

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

	AddTexture(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY,
		mStartX, mBackgroundY, 1.0f, 1.0f, LAYER_BACKGROUND, false);

	if (mBoard) {
		RegisterDrawCommand("Board",
			[this](SDL_Renderer* renderer) { mBoard->Draw(renderer); },
			LAYER_GAME_OBJECT);
		RegisterDrawCommand("Prompt",
			[this](SDL_Renderer* renderer) {
				if (!mPrompt.active) return;

				auto texture = ResourceManager::GetInstance().GetTexture(mPrompt.textureKey);
				if (!texture) return;

				int w, h;
				SDL_QueryTexture(texture, nullptr, nullptr, &w, &h);

				float screenWidth = SCENE_WIDTH;
				float screenHeight = SCENE_HEIGHT;
				float centerX = screenWidth / 2.0f;
				float centerY = screenHeight / 2.0f;

				float drawW = w * mPrompt.scale;
				float drawH = h * mPrompt.scale;
				SDL_FRect dest = { centerX - drawW / 2, centerY - drawH / 2, drawW, drawH };

				SDL_SetTextureAlphaMod(texture, mPrompt.alpha);
				SDL_RenderCopyF(renderer, texture, nullptr, &dest);
				SDL_SetTextureAlphaMod(texture, 255);
			},
			LAYER_UI + 1000);  
	}
	SortDrawCommands();
}

void GameScene::OnEnter() {
	Scene::OnEnter();
#ifdef _DEBUG
	std::cout << "进入游戏场景" << std::endl;
#endif
	// 加载背景
	mBoard = std::make_unique<Board>(this, 1);
	auto CardUI = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameObject>(
		LAYER_UI);
	CardUI->SetName("CardUI");
	mCardSlotManager = CardUI->AddComponent<CardSlotManager>(mBoard.get());

	mGameProgress = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameProgress>(
		LAYER_UI, mBoard.get(), this);
	mGameProgress->SetActive(false);
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);

	// TODO: 不要捕获局部变量的引用！ 这里会造成循环引用！
	/*
	auto button1 = mUIManager.CreateButton(Vector(100, 150));
	button1->SetAsCheckbox(true);
	button1->SetClickCallBack([](bool isChecked) {
		std::cout << "按钮1 被点击，当前状态: " << (isChecked ? "选中" : "未选中") << std::endl;
		}
	);

	auto slider = mUIManager.CreateSlider(Vector(300, 150), Vector(135, 10), 0.0f, 100.0f, 0.0f);
	slider->SetChangeCallBack([slider](float value) {
		}
	);
	*/
}

void GameScene::OnExit() {
	Scene::OnExit();
	std::cout << "退出GameScene" << std::endl;
	mBoard.reset();
	mGameProgress.reset();
	mCardSlotManager.reset();
	if (mChooseCardUI)
	{
		mChooseCardUI.reset();
	}
}

void GameScene::Update() {
	Scene::Update();
	if (mBoard)
	{
		mBoard->Update();

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

			// 计算背景应有的屏幕坐标（原逻辑）
			float screenX = mStartX + (mTargetSceneX - mStartX) * eased;

			// 背景世界坐标固定为 mStartX
			float worldX = mStartX;

			// 摄像机位置 = 世界坐标 - 屏幕坐标
			float camX = worldX - screenX;

			// 移动摄像机（保持 Y 轴不变）
			GameAPP::GetInstance().GetCamera().SetPosition(Vector(camX, 0));

			// 更新 mCurrectSceneX 供后续使用
			mCurrectSceneX = screenX;

			break;
		}
		case IntroStage::SEEDBANK_SLIDE:
		{
			// 首次进入时添加种子槽纹理（初始位置在屏幕外上方）
			if (!mSeedbankAdded) {
				AddTexture(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG,
					130.0f, -100.0f,			// 起始位置：x=120, y 上方
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
			if (!mSunCounterRegistered) {
				RegisterDrawCommand("SunCounter",
					[this](SDL_Renderer* renderer) {
						GameAPP::GetInstance().DrawText(std::to_string(mBoard->GetSun()),
							Vector(142, 42), SDL_Color{ 0,0,0,255 },
							ResourceKeys::Fonts::FONT_FZCQ, 17);
					},
					LAYER_UI + 100000);
				SortDrawCommands();
				mSunCounterRegistered = true;
			}
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

			GameAPP::GetInstance().GetCamera().SetPosition(Vector(camX, 0));

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
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager.get());
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI.reset();
	}

	RegisterDrawCommand("ZombieNumber",
		[this](SDL_Renderer* renderer) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawWorldText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(3, 569), SDL_Color{ 0,0,0,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 24);
			gameApp.DrawWorldText(u8"当前僵尸数量: " + std::to_string(mBoard->mZombieNumber),
				Vector(5, 570), SDL_Color{ 223,186,98,255 },
				ResourceKeys::Fonts::FONT_FZCQ, 24);
		},
		LAYER_UI);
	RegisterDrawCommand("LevelName",
		[this](SDL_Renderer* renderer) {
			auto& gameApp = GameAPP::GetInstance();
			gameApp.DrawWorldText(mBoard->mLevelName,
				Vector(766, 569), SDL_Color{ 0,0,0,255 },
				ResourceKeys::Fonts::FONT_CONTINUUMBOLD, 21);
			gameApp.DrawWorldText(mBoard->mLevelName,
				Vector(768, 570), SDL_Color{ 223,186,98,255 },
				ResourceKeys::Fonts::FONT_CONTINUUMBOLD, 21);
		},
		LAYER_UI);
	SortDrawCommands();
}

std::shared_ptr<GameProgress> GameScene::GetGameProgress() const
{
	return this->mGameProgress;
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
}