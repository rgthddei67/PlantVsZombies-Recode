#include "GameScene.h"
#include "SceneManager.h"
#include "../ResourceManager.h"
#include "./Plant/PlantType.h"
#include "./CardSlotManager.h"
#include "../ResourceKeys.h"
#include "../DeltaTime.h"
#include "./AudioSystem.h"
#include "ChooseCardUI.h"
#include <iostream>

#include "./Zombie/Zombie.h"

GameScene::GameScene() {
	std::cout << "游戏场景创建" << std::endl;
}

GameScene::~GameScene() {
	std::cout << "游戏场景销毁" << std::endl;
	if (mChooseCardUI)
	{
		mChooseCardUI.reset();
	}
}

void GameScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	// TODO: 根据传入的Background参数选择不同背景

	AddTexture(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY,
		mStartX, 0, 1.0f, 1.0f, LAYER_BACKGROUND);

	if (mBoard) {
		RegisterDrawCommand("Board",
			[this](SDL_Renderer* renderer) { mBoard->Draw(renderer); },
			LAYER_GAME_OBJECT);
	}
	SortDrawCommands();
}

void GameScene::OnEnter() {
	Scene::OnEnter();
#ifdef _DEBUG
	std::cout << "进入游戏场景" << std::endl;
#endif
	// 加载背景
	mBoard = std::make_unique<Board>();

	auto CardUI = GameObjectManager::GetInstance().CreateGameObjectImmediate<GameObject>(
		LAYER_UI);
	CardUI->SetName("CardUI");
	mCardSlotManager = CardUI->AddComponent<CardSlotManager>(mBoard.get());

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
	mCardSlotManager.reset();
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

			// 如果动画尚未开始或已完成，重置或继续计时
			if (!mHasEnter) {
				mAnimElapsed += deltaTime;
				if (mAnimElapsed >= mAnimDuration) {
					mAnimElapsed = mAnimDuration;
					mCurrentStage = IntroStage::SEEDBANK_SLIDE;
					mHasEnter = true;      // 动画结束
				}
			}

			// 计算动画进度 t (0~1)
			float t = mAnimElapsed / mAnimDuration;

			// 应用缓出函数：
			float eased = static_cast<float>((1 - cos(t * M_PI)) / 2);


			mCurrectSceneX = mStartX + (mTargetSceneX - mStartX) * eased;

			// 更新纹理位置
			SetTexturePosition(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY,
				mCurrectSceneX, 0);
			break;
		}
		case IntroStage::SEEDBANK_SLIDE:
		{
			// 首次进入时添加种子槽纹理（初始位置在屏幕外上方）
			if (!mSeedbankAdded) {
				mBoard->ShowPreviewZombies();
				AddTexture(ResourceKeys::Textures::IMAGE_SEEDBANK_LONG,
					130.0f, -100.0f,			// 起始位置：x=120, y 上方
					0.85f, 0.9f, LAYER_UI);
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
			float eased = static_cast<float>((1 - cos(t * M_PI)) / 2); // 与之前缓动一致
			Vector readyTargetPos = Vector(mGameStartX, 0);

			mCurrectSceneX = Vector::lerp(mReadyStartPos, readyTargetPos, eased).x;
			SetTexturePosition(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY, mCurrectSceneX, 0);

			if (mReadyAnimElapsed >= mReadyAnimDuration) {
				// 确保最终位置
				mCurrectSceneX = readyTargetPos.x;
				SetTexturePosition(ResourceKeys::Textures::IMAGE_BACKGROUND_DAY, mCurrectSceneX, 0);

				// 开始游戏
				if (mBoard) {
					mBoard->StartGame();
				}

				mCurrentStage = IntroStage::FINISH;
			}
			break;
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
	mBoard->DestroyPreviewZombies();
	mCurrentStage = IntroStage::READY_SET_PLANT;
	mReadyStartPos = Vector(mCurrectSceneX, 0);

	if (mChooseCardUI) {
		mChooseCardUI->TransferSelectedCardsTo(mCardSlotManager.get());
		mChooseCardUI->RemoveAllCards();
		GameObjectManager::GetInstance().DestroyGameObject(mChooseCardUI);
		mChooseCardUI.reset();
	}
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