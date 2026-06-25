#include "GameSelectScene.h"
#include "SceneManager.h"
#include "../GameApp.h"
#include "AudioSystem.h"
#include "Board.h"
#include "../Logger.h"
#include <SDL2/SDL_ttf.h>
#include <string>

namespace {
// 在以 (centerX,centerY) 为中心、最大宽度 maxWidth 的范围内绘制文字：
// 用 TTF_SizeUTF8 真实测量像素宽高，自最大字号往下试，直到放得下，再按真实宽高
// 水平+垂直居中。复刻 PlantAlmanacScene 的「TTF 测宽 + 字号自适应」思路。
void DrawFittedCenteredText(GameAPP& app, const std::string& text,
	float centerX, float centerY, float maxWidth, const glm::vec4& color,
	const std::string& fontKey, int maxSize, int minSize)
{
	int fs = minSize, tw = 0, th = 0;
	for (int s = maxSize; s >= minSize; --s) {
		TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, s);
		int w = 0, h = 0;
		if (font) TTF_SizeUTF8(font, text.c_str(), &w, &h);
		fs = s; tw = w; th = h;
		if (w <= maxWidth) break;   // 当前字号已能装下 → 采用
	}
	app.DrawText(text, Vector(centerX - tw * 0.5f, centerY - th * 0.5f),
		color, fontKey, fs);
}
} // namespace

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

	// ===== 关卡卡片：卡1=白天无尽(level 1000)、卡2=黑夜无尽(level 1001)；其余暂注释 =====
	const float kCardScale = 0.95f;           // Challenge_Window 原始 118x120
	const float kPitchX = 150.0f;             // 列间距
	const float kCol0X = 115.0f;              // 第一列左上角 x
	const float kRow1Y = 150.0f;              // 第 1 行 y
	// const float kRow2Y = 320.0f;           // 第 2 行 y（启用更多卡片时再用）
	const float kCardW = 118.0f * kCardScale;
	const float kCardH = 120.0f * kCardScale;

	// makeCard：点击置 mPendingEnterLevel(>=0)，由 Update 统一进 GameScene（不在回调内切场景）
	// groundKey：垫在卡框图标开口下的地面预览图（白天/黑夜），最底层、被卡框挡住只露开口
	auto makeCard = [this, kCardW, kCardH, kCardScale](float x, float y, int enterLevel,
		const std::string& groundKey) {
		// 底层地面图：填入卡框图标开口(native x[20..96] y[8..66] of 118x120)，
		// drawOrder -900(在羊皮纸 -1000 之上、卡框 LAYER_UI 之下)，被卡框只露开口
		if (const Texture* tex = ResourceManager::GetInstance().GetTexture(groundKey)) {
			const float bleed = 2.0f;                  // 略外扩，边缘掖进卡框边框下
			float ox = 20.0f * kCardScale - bleed;
			float oy = 8.0f * kCardScale - bleed;
			float ow = 77.0f * kCardScale + 2 * bleed;
			float oh = 59.0f * kCardScale + 2 * bleed;
			AddTexture(groundKey, x + ox, y + oy,
				ow / tex->width, oh / tex->height, -900, false);
		}

		auto card = mUIManager.CreateButton(Vector(x, y), Vector(kCardW, kCardH));
		card->SetAsCheckbox(false);
		card->SetImageKeys(
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT,
			ResourceKeys::Textures::IMAGE_CHALLENGE_WINDOW_HIGHLIGHT);
		card->SetClickCallBack([this, enterLevel](bool) {
			mPendingEnterLevel = enterLevel;
			});
		mCards.push_back(card);
	};

	makeCard(kCol0X + kPitchX * 0, kRow1Y, SURVIVAL_ENDLESS_LEVEL,
		"IMAGE_ALMANAC_GROUNDDAY");         // 卡1：白天无尽
	makeCard(kCol0X + kPitchX * 1, kRow1Y, SURVIVAL_ENDLESS_NIGHT_LEVEL,
		"IMAGE_ALMANAC_GROUNDNIGHT");       // 卡2：黑夜无尽

	// 多余的关卡方框（暂注释，后续接入更多模式时再启用；启用时同步上方 kLabels 与 kRow2Y）：
	// makeCard(kCol0X + kPitchX * 2, kRow1Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 3, kRow1Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 4, kRow1Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 5, kRow1Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 0, kRow2Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 1, kRow2Y, /* level */ -1);
	// makeCard(kCol0X + kPitchX * 2, kRow2Y, /* level */ -1);

	// ===== 顶部标题 + 9 个占位标签：TTF_SizeUTF8 真实测宽自适应居中（参考 PlantAlmanacScene） =====
	RegisterDrawCommand("DrawSelectTexts",
		[kCol0X, kPitchX, kRow1Y, kCardW, kCardH](Graphics* g) {
			auto& gameApp = GameAPP::GetInstance();

			// 顶部标题：居中于木牌中心 (551, 88)，木牌内宽约 560，自适应字号 42→24
			DrawFittedCenteredText(gameApp, u8"选择关卡", 551.0f, 88.0f, 500.0f,
				glm::vec4(248, 236, 122, 255), ResourceKeys::Fonts::FONT_FZJZ, 42, 24);

			// 卡片标签：与上面 makeCard 顺序一致（卡1白天、卡2黑夜），居中于灰色标签条
			static const char* kLabels[2] = { u8"白天无尽", u8"黑夜无尽" };
			auto drawLabel = [&](int index, float x, float y) {
				float cx = x + kCardW * 0.5f;
				float cy = y + kCardH * 0.73f;
				DrawFittedCenteredText(gameApp, kLabels[index], cx, cy, kCardW * 0.82f,
					glm::vec4(70, 60, 40, 255), ResourceKeys::Fonts::FONT_FZJZ, 16, 9);
			};
			for (int i = 0; i < 2; ++i) drawLabel(i, kCol0X + kPitchX * i, kRow1Y);
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
	if (mPendingEnterLevel >= 0) {
		int enterLevel = mPendingEnterLevel;
		mPendingEnterLevel = -1;
		auto& gameApp = GameAPP::GetInstance();
		auto& sceneMgr = SceneManager::GetInstance();
		gameApp.GetGraphics().SetCameraPosition(0, 0);
		sceneMgr.SetGlobalData("EnterLevel", std::to_string(enterLevel));
		sceneMgr.SwitchTo("GameScene");
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
