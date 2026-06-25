#include "GameSelectScene.h"
#include "SceneManager.h"
#include "../GameApp.h"
#include "AudioSystem.h"
#include "../Logger.h"

void GameSelectScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();

	// 背景：Challenge_Background.png 为 1280x720，缩放铺满 1100x600 逻辑画面
	AddTexture(ResourceKeys::Textures::IMAGE_CHALLENGE_BACKGROUND,
		0.0f, 0.0f, 1100.0f / 1280.0f, 600.0f / 720.0f, -1000, false);

	// 左下角「返回菜单」按钮（位置/尺寸/样式照搬 AlmanacScene）
	mBackMenuButton = mUIManager.CreateButton(Vector(7, 560), Vector(162, 26));
	mBackMenuButton->SetAsCheckbox(false);
	mBackMenuButton->SetImageKeys(
		"IMAGE_ALMANAC_INDEXBUTTON",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT");
	mBackMenuButton->SetText(u8"返回菜单", ResourceKeys::Fonts::FONT_FZJZ, 18);
	mBackMenuButton->SetTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetHoverTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetClickCallBack([this](bool) {
		this->mReadyToSwitchMainMenu = true;
		});

	// ===== 9 张占位关卡卡片：第 1 行 6 张、第 2 行 3 张（复刻参考图） =====
	const float kCardScale = 0.95f;           // Challenge_Window 原始 118x120
	const float kPitchX = 150.0f;             // 列间距
	const float kCol0X = 115.0f;              // 第一列左上角 x
	const float kRow1Y = 150.0f;              // 第 1 行 y
	const float kRow2Y = 320.0f;              // 第 2 行 y
	const float kCardW = 118.0f * kCardScale;
	const float kCardH = 120.0f * kCardScale;

	auto makeCard = [this, kCardW, kCardH](int index, float x, float y) {
		auto card = mUIManager.CreateButton(Vector(x, y), Vector(kCardW, kCardH));
		card->SetAsCheckbox(false);
		card->SetImageKeys(
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT);
		card->SetClickCallBack([index](bool) {
			// 纯视觉脚手架：点击仅打日志，不跳转（Release 下 INFO 被裁，等同 no-op）
			LOG_INFO("UI") << "GameSelectScene card " << index << " clicked";
			});
		mCards.push_back(card);
	};

	for (int i = 0; i < 6; ++i) {             // 第 1 行 6 张
		makeCard(i, kCol0X + kPitchX * i, kRow1Y);
	}
	for (int i = 0; i < 3; ++i) {             // 第 2 行 3 张（左对齐前 3 列）
		makeCard(6 + i, kCol0X + kPitchX * i, kRow2Y);
	}

	// ===== 顶部标题 + 9 个占位标签（每帧绘制；坐标为起始值，按截图微调） =====
	RegisterDrawCommand("DrawSelectTexts",
		[kCol0X, kPitchX, kRow1Y, kRow2Y, kCardW, kCardH](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();

			// 顶部标题：居中于背景横幅之上（x/y 起始值，截图后微调）
			gameApp.DrawText(u8"选择关卡", Vector(470, 28),
				glm::vec4(248, 236, 122, 255));

			// 9 个占位标签「关卡N」，画在每张卡下方小条中央
			static const char* kLabels[9] = {
				u8"关卡一", u8"关卡二", u8"关卡三", u8"关卡四", u8"关卡五",
				u8"关卡六", u8"关卡七", u8"关卡八", u8"关卡九" };
			const float kLabelFontApproxW = 13.0f;   // 估算每个汉字宽度，用于水平居中
			auto drawLabel = [&](int index, float x, float y) {
				float cx = x + kCardW * 0.5f;
				float cy = y + kCardH - 18.0f;        // 下方小条
				float textW = 3 * kLabelFontApproxW;  // "关卡N" 3 字
				gameApp.DrawText(kLabels[index], Vector(cx - textW * 0.5f, cy),
					glm::vec4(70, 60, 40, 255));
			};
			for (int i = 0; i < 6; ++i) drawLabel(i, kCol0X + kPitchX * i, kRow1Y);
			for (int i = 0; i < 3; ++i) drawLabel(6 + i, kCol0X + kPitchX * i, kRow2Y);
		},
		LAYER_UI + 100);

	SortDrawCommands();
}

void GameSelectScene::Update()
{
	Scene::Update();

	if (mReadyToSwitchMainMenu) {
		mReadyToSwitchMainMenu = false;
		SceneManager::GetInstance().SwitchTo("MainMenuScene");
		return;
	}
}

void GameSelectScene::OnEnter()
{
	Scene::OnEnter();
	AudioSystem::PlayMusic(ResourceKeys::Music::MUSIC_CHOOSEYOURSEEDS, -1);
}

void GameSelectScene::OnExit()
{
	mBackMenuButton.reset();
	mCards.clear();
	Scene::OnExit();
}
