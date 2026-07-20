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
#include "Perk/SurvivalPerkManager.h"
#include "../GameAPP.h"
#include "../Graphics.h"
#include "../Profiler.h"
#include "./Shovel.h"
#include "ShovelBank.h"
#include "../Logger.h"
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace {
	// 右下角关卡名/轮数显示。
	// 冒险模式：沿用原左对齐（左端锚点 x=768，阴影 766），不改动既有观感。
	// 生存模式：右对齐——右端锚点固定，文字越长越向左延伸，避免"第10面旗"等长文本撞到右侧"难度"文字。
	// 右锚点取 1020：第1轮文本宽约 252px，drawX≈768，与原左对齐位置基本重合，故第1轮观感不变。
	constexpr float kLevelNameRightAnchor = 865.0f;
	constexpr float kWeatherPanelWidth = 286.0f;          // 天气面板宽度（逻辑像素）
	constexpr float kWeatherPanelHeight = 72.0f;          // 天气面板高度（逻辑像素）
	constexpr float kWeatherPanelVisibleX = 12.0f;        // 完全滑入后的左边距（逻辑像素）
	constexpr float kWeatherPanelY = 76.0f;               // 面板顶部位置，避开上方种子槽（逻辑像素）
	constexpr float kWeatherPanelSlideDuration = 0.32f;   // 完整滑入或滑出的动画时长（秒，未缩放）
	constexpr float kCurrentWeatherNoticeDuration = 5.0f; // 天气揭晓后继续展示“当前天气”的时长（秒，未缩放）
	constexpr int kWeatherCurrentFontSize = 18;           // 第一行“当前天气”字号
	constexpr int kWeatherForecastFontSize = 16;          // 第二行“天气预警”字号
	constexpr float kForecastFailureWidth = 286.0f;       // 预报失败提示宽度（逻辑像素）
	constexpr float kForecastFailureHeight = 58.0f;       // 预报失败提示高度（逻辑像素）
	constexpr float kForecastFailureY = 154.0f;           // 失败提示顶部位置，显示在天气面板下方（逻辑像素）
	constexpr float kForecastFailureDuration = 3.2f;      // 失败提示从出现到完全消失的总时长（秒，未缩放）
	constexpr float kForecastFailureAppearDuration = 0.25f; // 失败提示滑入动画时长（秒，未缩放）
	constexpr float kForecastFailureFadeDuration = 0.45f; // 失败提示末尾淡出时长（秒，未缩放）
	constexpr int kForecastFailureTitleFontSize = 17;     // “天气预报失败”标题字号
	constexpr int kForecastFailureDetailFontSize = 15;    // 预报与实际天气对照行字号

#define DEVZ(n) { ZombieType::n, #n }
	// 开发者面板循环切换表；显示枚举标识符（简陋版足够）。与 TestDriver kZombieNames 同集合。
	const std::vector<std::pair<ZombieType, const char*>> kDevZombieTable = {
		DEVZ(ZOMBIE_NORMAL), DEVZ(ZOMBIE_TRAFFIC_CONE), DEVZ(ZOMBIE_POLEVAULTER), DEVZ(ZOMBIE_BUCKET),
		DEVZ(ZOMBIE_FASTBUCKET), DEVZ(ZOMBIE_NEWSPAPER), DEVZ(ZOMBIE_FASTPAPER), DEVZ(ZOMBIE_DOOR),
		DEVZ(ZOMBIE_FOOTBALL), DEVZ(ZOMBIE_DANCER), DEVZ(ZOMBIE_BACKUP_DANCER), DEVZ(ZOMBIE_PINK_FOOTBALL),
		DEVZ(ZOMBIE_DUCKY_TUBE),
		DEVZ(ZOMBIE_SNORKEL), DEVZ(ZOMBIE_ZAMBONI), DEVZ(ZOMBIE_BOBSLED), DEVZ(ZOMBIE_DOLPHIN_RIDER),
		DEVZ(ZOMBIE_JACK_IN_THE_BOX), DEVZ(ZOMBIE_BALLOON), DEVZ(ZOMBIE_DIGGER), DEVZ(ZOMBIE_POGO),
		DEVZ(ZOMBIE_YETI), DEVZ(ZOMBIE_BUNGEE), DEVZ(ZOMBIE_LADDER), DEVZ(ZOMBIE_CATAPULT),
		DEVZ(ZOMBIE_GARGANTUAR), DEVZ(ZOMBIE_IMP), DEVZ(ZOMBIE_BOSS), DEVZ(ZOMBIE_PEA_HEAD),
		DEVZ(ZOMBIE_WALLNUT_HEAD), DEVZ(ZOMBIE_JALAPENO_HEAD), DEVZ(ZOMBIE_GATLING_HEAD),
		DEVZ(ZOMBIE_SQUASH_HEAD), DEVZ(ZOMBIE_TALLNUT_HEAD), DEVZ(ZOMBIE_REDEYE_GARGANTUAR),
	};
#undef DEVZ

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

	/** 把内部雨势枚举转换为面板使用的简短中文名称。 */
	const char* RainIntensityDisplayName(RainIntensity intensity) {
		switch (intensity) {
		case RainIntensity::CLEAR:  return u8"晴天";
		case RainIntensity::LIGHT:  return u8"小雨";
		case RainIntensity::MEDIUM: return u8"中雨";
		case RainIntensity::HEAVY:  return u8"大雨";
		}
		return u8"未知";
	}

	/** 返回各档天气在面板上的强调色，并保留调用方提供的透明度。 */
	glm::vec4 RainIntensityTextColor(RainIntensity intensity, float alpha) {
		switch (intensity) {
		case RainIntensity::CLEAR:  return glm::vec4(164.0f, 224.0f, 145.0f, alpha);
		case RainIntensity::LIGHT:  return glm::vec4(154.0f, 214.0f, 255.0f, alpha);
		case RainIntensity::MEDIUM: return glm::vec4(92.0f, 169.0f, 255.0f, alpha);
		case RainIntensity::HEAVY:  return glm::vec4(255.0f, 166.0f, 116.0f, alpha);
		}
		return glm::vec4(230.0f, 230.0f, 230.0f, alpha);
	}
}

GameScene::GameScene() {
}

GameScene::~GameScene() {
}

void GameScene::Draw(Graphics* g)
{
	// 屏幕抖动走相机（m_viewMatrix → projView push constant）而非变换栈：
	// reanim/血量字形的 GPU instancing 快路径不消费变换栈（InstanceRecord 直写世界坐标，
	// 见 Animator::DrawInternalInstanced / AppendReanimInstance），栈方案只能平移 batch
	// 路径的背景与 UI，植物/僵尸留在原位；projView 是全部管线（batch/instance/字形/
	// 延迟文字）的公共出口，相机平移一次覆盖，且视口剔除与 LogicalToWorld 自动一致。
	// 注意：不能每帧无条件写相机——开场动画的背景平移也在用 SetCameraPosition（本文件
	// Update 内 camX 路径）。抖动只发生在 GAME 状态（此时相机基线恒为 0），故只在
	// 抖动中覆写、结束后归零一次。相机设定须在 Scene::Draw 之前完成，保证本帧所有
	// flush（含 mid-frame blend 切换触发的）读到同一个 projView。
	const Vector shake = mBoard ? mBoard->GetShakeOffset() : Vector(0.0f, 0.0f);
	const bool shaking = (shake.x != 0.0f || shake.y != 0.0f);
	if (shaking) {
		g->SetCameraPosition(-shake.x, -shake.y);   // view 平移 = -cameraPos，场景整体移 +shake
		mShakeCameraApplied = true;
	}
	else if (mShakeCameraApplied) {
		g->SetCameraPosition(0.0f, 0.0f);
		mShakeCameraApplied = false;
	}
	Scene::Draw(g);
}

void GameScene::DrawWorldOverlay(Graphics* g)
{
	if (!g || !mBoard) return;
	const float alpha = mBoard->GetRainOverlayAlpha();
	if (alpha <= 0.0f) return;

	// 低成本雨天环境光：只做蓝灰半透明暗幕。该钩子位于战场主体之后、UI overlay 之前，
	// 因而植物/僵尸/背景一起变暗，但卡片、按钮和文字保持清晰。
	g->FillRect(0.0f, 0.0f,
		static_cast<float>(SCENE_WIDTH), static_cast<float>(SCENE_HEIGHT),
		glm::vec4(36.0f, 52.0f, 78.0f, alpha));
}

/** 用未缩放时间推进天气面板与失败提示，使游戏倍速不改变 UI 动画观感。 */
void GameScene::UpdateWeatherUi(float deltaTime)
{
	const bool shouldShow = mBoard && mBoard->mBoardState == BoardState::GAME
		&& (mBoard->HasWeatherForecast() || mCurrentWeatherNoticeTimer > 0.0f);
	const float direction = shouldShow ? 1.0f : -1.0f;
	mWeatherPanelSlide = std::clamp(mWeatherPanelSlide
		+ direction * deltaTime / kWeatherPanelSlideDuration, 0.0f, 1.0f);
	if (mCurrentWeatherNoticeTimer > 0.0f) {
		mCurrentWeatherNoticeTimer = std::max(0.0f,
			mCurrentWeatherNoticeTimer - deltaTime);
	}
	if (mWeatherForecastFailureTimer > 0.0f) {
		mWeatherForecastFailureTimer = std::max(0.0f,
			mWeatherForecastFailureTimer - deltaTime);
	}
}

/** 在左上角绘制当前天气与已锁定的下一天气预警。 */
void GameScene::DrawWeatherPanel(Graphics* g) const
{
	if (!g || !mBoard || mWeatherPanelSlide <= 0.0f) return;

	const float eased = mWeatherPanelSlide * mWeatherPanelSlide
		* (3.0f - 2.0f * mWeatherPanelSlide);
	const float x = -kWeatherPanelWidth
		+ (kWeatherPanelWidth + kWeatherPanelVisibleX) * eased;
	const float alpha = 255.0f * eased;

	// 深蓝半透明底板配强度色边条；矩形方案不新增贴图，分辨率和全屏模式都保持锐利。
	g->FillRect(x + 3.0f, kWeatherPanelY + 3.0f,
		kWeatherPanelWidth, kWeatherPanelHeight,
		glm::vec4(0.0f, 0.0f, 0.0f, 92.0f * eased));
	g->FillRect(x, kWeatherPanelY, kWeatherPanelWidth, kWeatherPanelHeight,
		glm::vec4(18.0f, 28.0f, 48.0f, 218.0f * eased));
	g->DrawRect(x, kWeatherPanelY, kWeatherPanelWidth, kWeatherPanelHeight,
		glm::vec4(111.0f, 151.0f, 196.0f, 180.0f * eased));
	g->FillRect(x, kWeatherPanelY, 5.0f, kWeatherPanelHeight,
		RainIntensityTextColor(mBoard->GetRainIntensity(), alpha));

	const std::string currentLine = std::string(u8"当前天气：")
		+ RainIntensityDisplayName(mBoard->GetRainIntensity());
	std::string forecastLine = u8"天气预警：暂无";
	glm::vec4 forecastColor(166.0f, 178.0f, 196.0f, alpha);
	if (mBoard->HasWeatherForecast()) {
		const int seconds = std::max(0, static_cast<int>(std::ceil(mBoard->GetWeatherTimer())));
		forecastLine = std::string(u8"天气预警（") + std::to_string(seconds)
			+ u8"秒）：" + RainIntensityDisplayName(mBoard->GetForecastRainIntensity());
		forecastColor = RainIntensityTextColor(mBoard->GetForecastRainIntensity(), alpha);
	}

	const float textX = x + 18.0f;
	const glm::vec4 shadow(0.0f, 0.0f, 0.0f, 185.0f * eased);
	g->DrawText(currentLine, ResourceKeys::Fonts::FONT_FZCQ, kWeatherCurrentFontSize,
		shadow, textX + 1.0f, kWeatherPanelY + 10.0f);
	g->DrawText(currentLine, ResourceKeys::Fonts::FONT_FZCQ, kWeatherCurrentFontSize,
		RainIntensityTextColor(mBoard->GetRainIntensity(), alpha), textX, kWeatherPanelY + 9.0f);
	g->DrawText(forecastLine, ResourceKeys::Fonts::FONT_FZCQ, kWeatherForecastFontSize,
		shadow, textX + 1.0f, kWeatherPanelY + 42.0f);
	g->DrawText(forecastLine, ResourceKeys::Fonts::FONT_FZCQ, kWeatherForecastFontSize,
		forecastColor, textX, kWeatherPanelY + 41.0f);
}

/** 在天气面板下方绘制错误揭晓提示；它只提示结果，不抢输入也不暂停游戏。 */
void GameScene::DrawWeatherForecastFailure(Graphics* g) const
{
	if (!g || mWeatherForecastFailureTimer <= 0.0f) return;

	const float elapsed = kForecastFailureDuration - mWeatherForecastFailureTimer;
	const float appear = std::clamp(elapsed / kForecastFailureAppearDuration, 0.0f, 1.0f);
	const float fade = std::clamp(mWeatherForecastFailureTimer
		/ kForecastFailureFadeDuration, 0.0f, 1.0f);
	const float visibility = std::min(appear, fade);
	const float eased = appear * appear * (3.0f - 2.0f * appear);
	const float x = -kForecastFailureWidth
		+ (kForecastFailureWidth + kWeatherPanelVisibleX) * eased;
	const float alpha = 255.0f * visibility;

	// 暖红色与上方天气面板的冷色区分开，玩家无需读完文字也能识别“预报失准”。
	g->FillRect(x + 3.0f, kForecastFailureY + 3.0f,
		kForecastFailureWidth, kForecastFailureHeight,
		glm::vec4(0.0f, 0.0f, 0.0f, 88.0f * visibility));
	g->FillRect(x, kForecastFailureY, kForecastFailureWidth, kForecastFailureHeight,
		glm::vec4(58.0f, 25.0f, 29.0f, 224.0f * visibility));
	g->DrawRect(x, kForecastFailureY, kForecastFailureWidth, kForecastFailureHeight,
		glm::vec4(255.0f, 135.0f, 121.0f, 205.0f * visibility));
	g->FillRect(x, kForecastFailureY, 5.0f, kForecastFailureHeight,
		glm::vec4(255.0f, 105.0f, 91.0f, alpha));

	const std::string title = u8"天气预报失败！";
	const std::string detail = std::string(u8"预报：")
		+ RainIntensityDisplayName(mFailedForecastRainIntensity)
		+ u8"  →  实际：" + RainIntensityDisplayName(mActualForecastRainIntensity);
	const float textX = x + 18.0f;
	const glm::vec4 shadow(0.0f, 0.0f, 0.0f, 185.0f * visibility);
	g->DrawText(title, ResourceKeys::Fonts::FONT_FZCQ, kForecastFailureTitleFontSize,
		shadow, textX + 1.0f, kForecastFailureY + 7.0f);
	g->DrawText(title, ResourceKeys::Fonts::FONT_FZCQ, kForecastFailureTitleFontSize,
		glm::vec4(255.0f, 181.0f, 169.0f, alpha), textX, kForecastFailureY + 6.0f);
	g->DrawText(detail, ResourceKeys::Fonts::FONT_FZCQ, kForecastFailureDetailFontSize,
		shadow, textX + 1.0f, kForecastFailureY + 34.0f);
	g->DrawText(detail, ResourceKeys::Fonts::FONT_FZCQ, kForecastFailureDetailFontSize,
		glm::vec4(242.0f, 229.0f, 220.0f, alpha), textX, kForecastFailureY + 33.0f);
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

	// 开发者模式常驻角标（左上角小字；暂停刷怪时附加状态）
	if (GameAPP::mDevelopMode) {
		RegisterDrawCommand("DevModeBadge",
			[](Graphics* g) {
				std::string badge = u8"开发者模式";
				if (GameAPP::mDevSpawnPaused) badge += u8"（刷怪已暂停）";
				auto& gameApp = GameAPP::GetInstance();
				gameApp.DrawText(badge, Vector(4, 4), { 0,0,0,255 },
					ResourceKeys::Fonts::FONT_FZCQ, 14);
				gameApp.DrawText(badge, Vector(5, 5), { 255, 90, 90, 255 },
					ResourceKeys::Fonts::FONT_FZCQ, 14);
			},
			LAYER_UI + 100000);
	}

	if (mBoard) {
		RegisterDrawCommand("WeatherPanel",
			[this](Graphics* g) { DrawWeatherPanel(g); },
			LAYER_UI + 500);
		RegisterDrawCommand("WeatherForecastFailure",
			[this](Graphics* g) { DrawWeatherForecastFailure(g); },
			LAYER_UI + 600);

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

		// 全屏白闪（寒冰菇）：盖过场景与 Prompt，但在开发者角标（+100000）之下
		RegisterDrawCommand("ScreenFlash",
			[this](Graphics* g) {
				if (mScreenFlashTimer <= 0.0f) return;
				// 峰值可由调用方控制：寒冰菇沿用 200，大雨闪电使用更柔和的短闪。
				const float t = mScreenFlashTimer / mScreenFlashDuration;
				glm::vec4 color(255.0f, 255.0f, 255.0f, mScreenFlashPeakAlpha * t);
				g->FillRect(0.0f, 0.0f,
					static_cast<float>(SCENE_WIDTH), static_cast<float>(SCENE_HEIGHT), color);
			},
			LAYER_UI + 2000);

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

	// 生存模式专属：右上角第三个按钮「词条」，点开已选词条查看面板（普通关卡不创建）
	if (mBoard->mIsSurvival) {
		auto button3 = mUIManager.CreateButton(Vector(990, 95), Vector(125 * 0.9f, 52 * 0.9f));
		mPerkViewButton = button3;
		button3->SetText(u8"词条");
		button3->SetAsCheckbox(false);
		button3->SetTextColor(glm::vec4{ 53, 191, 61, 255 });
		button3->SetHoverTextColor(glm::vec4{ 53, 240, 61, 255 });
		button3->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);
		button3->SetClickCallBack([this](bool) { this->OpenPerkView(); });
	}

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
	mPerkViewButton.reset();
	mGameProgress = nullptr;
	mCardSlotManager = nullptr;
	mChooseCardUI = nullptr;
}

void GameScene::OpenMenu()
{
	if (mOpenMenu) return;
	if (mSurvivalPerkSelectActive) return;   // 选词条模态期间禁止打开暂停菜单（否则会在框下解除暂停）
	if (mPerkViewActive) return;             // 词条查看面板打开期间禁止叠开暂停菜单

	mOpenMenu = true;
	DeltaTime::SetPaused(true);
	auto& gameApp = GameAPP::GetInstance();
	const glm::vec4 labelColor{ 107, 109, 144, 255 };
	mMenu = GameMessageBox::Builder(Vector(SCENE_WIDTH / 2 + 50, SCENE_HEIGHT / 2 - 80.0f))
		.Background(ResourceKeys::Textures::IMAGE_OPTIONS_MENUBACK)
		.Button(u8"返回游戏", Vector(400, 430), Vector(360, 100), 40, [this]() {
			mOpenMenu = false;
			DeltaTime::SetPaused(false);
		}, ResourceKeys::Textures::IMAGE_OPTIONS_BACKTOGAMEBUTTON0)
		.Button(u8"重新开始", Vector(485, 330), Vector(213 * 0.9f, 50 * 0.9f), 21,
			[this]() { this->OpenRestartMenu(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Button(u8"主菜单", Vector(485, 371), Vector(213 * 0.9f, 50 * 0.9f), 21,
			[this]() { this->OpenQuitMenu(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Button(u8"查看图鉴", Vector(485, 289), Vector(213 * 0.9f, 50 * 0.9f), 21, [this]() {
			DeltaTime::SetPaused(false);
			this->mLendToAlmanacScene = true;
		}, ResourceKeys::Textures::IMAGE_BUTTONBIG)
		.Checkbox(Vector(455, 250), Vector(42, 39), []() {
			auto& app = GameAPP::GetInstance();
			app.mShowPlantHP = !app.mShowPlantHP;
		}, gameApp.mShowPlantHP)
		.Checkbox(Vector(590, 250), Vector(42, 39), []() {
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
		.Text(Vector(498, 256), 14, u8"植物血量显示", labelColor)
		.Text(Vector(634, 256), 14, u8"僵尸血量显示", labelColor)
		.Show();
}

void GameScene::OpenRestartMenu()
{
	if (this->mOpenRestartMenu) return;
	this->mOpenRestartMenu = true;

	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Message(u8"    确定重新开始游戏吗?")
		.Scale(1.5f)
		.Button(u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mOpenMenu = false;
			this->mOpenRestartMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToRestart = true;
			this->mOpenRestartMenu = false;
			this->mOpenMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Show();
}

void GameScene::OpenQuitMenu()
{
	if (this->mOpenQuitMenu) return;
	this->mOpenQuitMenu = true;

	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Message(u8"    确定退出这把游戏吗?")
		.Scale(1.5f)
		.Button(u8"取消", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mOpenMenu = false;
			this->mOpenQuitMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"确定", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToBackMenu = true;
			this->mOpenQuitMenu = false;
			this->mOpenMenu = false;
			DeltaTime::SetPaused(false);
		})
		.Show();
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
		UpdateWeatherUi(DeltaTime::GetUnscaledDeltaTime());

		// 轮清后存档：BeginSurvivalCardSelect 置位，在 Board::Update 返回后（已脱离 Die() 调用栈）执行。
		// 触发轮清的那只濒死僵尸此刻仍在 EntityManager 中，由 SaveLevelData 内的 IsActive() 过滤排除。
		if (mPendingSurvivalSave) {
			mPendingSurvivalSave = false;
			GameAPP::GetInstance().mGameInfoSaver.SaveLevelData(mBoard.get(), mCardSlotManager);
		}

		auto& input = GameAPP::GetInstance().GetInputHandler();

		// 开发者模式：RSHIFT 键呼出/关闭面板（放置模式时按 SDLK_RSHIFT 回面板）
		if (GameAPP::mDevelopMode && input.IsKeyPressed(SDLK_RSHIFT)
			&& !mOpenMenu && !mSurvivalPerkSelectActive && !mPerkViewActive) {
			if (mDevSpawnMode)        { mDevSpawnMode = false; OpenDevPanel(); }
			else if (mDevPanelActive) { CloseDevPanel(); }
			else                      { OpenDevPanel(); }
		}

		// 开发者：召唤放置模式——独占 ESC 与左键（须在暂停菜单键处理之前）
		bool devConsumedEsc = false;
		if (mDevSpawnMode && !DeltaTime::IsPaused()) {
			if (input.IsKeyPressed(SDLK_ESCAPE)) {
				mDevSpawnMode = false;
				devConsumedEsc = true;
			}
			else if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
				const Vector mp = input.GetMouseWorldPosition();
				int bestRow = 0;
				float bestDist = 1e9f;
				for (int r = 0; r < mBoard->mRows; ++r) {
					const float d = std::abs(mp.y - mBoard->GetZombieSpawnY(r));
					if (d < bestDist) { bestDist = d; bestRow = r; }
				}
				mBoard->CreateZombie(kDevZombieTable[mDevZombieIndex].first, bestRow, mp.x);
				LOG_DEBUG("DevMode") << "召唤 " << kDevZombieTable[mDevZombieIndex].second
					<< " row=" << bestRow << " x=" << mp.x;
			}
		}

		if (mBoard->mBoardState != BoardState::LOSE_GAME && !this->mOpenRestartMenu && !devConsumedEsc && (input.IsKeyPressed(SDLK_SPACE) || input.IsKeyPressed(SDLK_ESCAPE))) {
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

						// 切回音乐
						mBoard->PlayBackgroundMusic();
					}
					// 恢复铲子显示
					if (mShovelUI) mShovelUI->SetActive(true);
					if (auto shovel = mBoard->mShovel.lock()) shovel->SetActive(true);

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
		// 全屏白闪衰减
		if (mScreenFlashTimer > 0.0f)
		{
			mScreenFlashTimer -= DeltaTime::GetDeltaTime();
			if (mScreenFlashTimer < 0.0f) mScreenFlashTimer = 0.0f;
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

	if (mDevPendingLevel >= 0) {
		const int lv = mDevPendingLevel;
		mDevPendingLevel = -1;
		GameAPP::GetInstance().GetGraphics().SetCameraPosition(0, 0);
		auto& sm = SceneManager::GetInstance();
		sm.SetGlobalData("EnterLevel", std::to_string(lv));
		sm.SwitchTo("GameScene");   // 重建 GameScene，不检查存档解锁
		return;
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

void GameScene::BeginSurvivalPerkSelect()
{
	if (!mBoard) return;

	// Board 已切到 CHOOSE_CARD，退出场景会立即保存；实际清空卡槽虽在词条结算后执行，
	// 但冷却快照必须现在就独立出来，保证词条界面点 X 后仍能恢复上一轮未完成的冷却。
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

	// 重新进入流程前先收掉可能残留的测试/旧 UI，确保一轮只有一个活动选择框。
	CloseSurvivalPerkSelectBox();
	mSurvivalPerkStepsCompleted = 0;
	mSurvivalPerkPicksCompleted = 0;
	mSurvivalPerkRefreshesRemaining = SURVIVAL_PERK_REFRESHES_PER_ROUND;
	mSurvivalPerkSelectActive = true;
	DeltaTime::SetPaused(true);
	RenderSurvivalPerkSelectStep();
}

void GameScene::CloseSurvivalPerkSelectBox()
{
	if (auto box = mPerkSelectBox.lock()) {
		box->SetActive(false);
		box->Close();
	}
	mPerkSelectBox.reset();
}

void GameScene::RenderSurvivalPerkSelectStep()
{
	if (!mBoard) return;

	mCurrentPerkOffer = RollPerkPairings(mBoard->GetPerkManager(), 3);

	auto& pm = mBoard->GetPerkManager();

	// 与 DrawText 同字体同字号量出逻辑像素宽（取不到字体时按半宽兜底）
	auto measureW = [](const std::string& s, int fontSize) -> float {
		TTF_Font* f = ResourceManager::GetInstance().GetFont(ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		if (!f) return static_cast<float>(s.size()) * fontSize * 0.5f;
		int w = 0, h = 0;
		TTF_SizeUTF8(f, s.c_str(), &w, &h);
		return static_cast<float>(w);
	};

	// 版面常量——盒子宽高完全由内容自动决定
	const int   titleFont  = 22;
	const int   rowFont    = 16;
	const float padX       = 30.0f;
	const float padY       = 22.0f;
	const float titleLineH = 30.0f;
	const float titleGap   = 16.0f;
	const float lineH      = 22.0f;            // 单行文字行高
	const float rowBlockH  = lineH * 2.0f;     // 每个配对两行（植物/僵尸）
	const float rowGap     = 20.0f;            // 配对之间的间隔
	const float gapTextBtn = 20.0f;            // 文字与「选择」按钮的间隔
	const float actionGap  = 20.0f;            // 配对区与底部操作按钮的间隔
	const float buttonGap  = 20.0f;            // 「刷新」与「放弃本次」之间的间隔
	const Vector selectBtnSize(100.0f, 40.0f);
	const Vector refreshBtnSize(210.0f, 44.0f);
	const Vector skipBtnSize(160.0f, 44.0f);
	const glm::vec4 green{ 53, 191, 61, 255 };
	const glm::vec4 red  { 200, 60, 60, 255 };
	const glm::vec4 titleColor{ 245, 214, 127, 255 };

	const std::string title = std::string(u8"第 ") + std::to_string(mBoard->mSurvivalRound - 1)
		+ u8" 轮 · 选择强化（第 " + std::to_string(mSurvivalPerkStepsCompleted + 1)
		+ u8"/" + std::to_string(SURVIVAL_PERK_PICKS_PER_ROUND) + u8" 次）";

	// 预生成每个配对的两行文字并量宽，求内容最大宽度（descZh 已自带词条名，不再叠加 nameZh）
	struct Row { std::string plant; std::string zombie; };
	std::vector<Row> rows;
	rows.reserve(mCurrentPerkOffer.size());
	float maxRowW = 0.0f;
	for (const PerkPairing& pr : mCurrentPerkOffer) {
		const PerkInfo& bp = SurvivalPerkManager::GetInfo(pr.plant);
		const PerkInfo& cz = SurvivalPerkManager::GetInfo(pr.zombie);
		Row r;
		r.plant  = std::string(u8"植物：") + bp.descZh + u8"（当前 " + std::to_string(pm.GetStacks(pr.plant)) + u8" 层）";
		r.zombie = std::string(u8"僵尸：") + cz.descZh + u8"（当前 " + std::to_string(pm.GetStacks(pr.zombie)) + u8" 层）";
		float wp = measureW(r.plant, rowFont);
		float wz = measureW(r.zombie, rowFont);
		float w = (wp > wz) ? wp : wz;
		if (w > maxRowW) maxRowW = w;
		rows.push_back(r);
	}

	const float titleW = measureW(title, titleFont);
	float contentW = maxRowW + gapTextBtn + selectBtnSize.x;
	if (titleW > contentW)        contentW = titleW;
	const float actionButtonsW = refreshBtnSize.x + buttonGap + skipBtnSize.x;
	if (actionButtonsW > contentW) contentW = actionButtonsW;

	const int   N     = static_cast<int>(rows.size());
	const float rowsH = (N > 0) ? (N * rowBlockH + (N - 1) * rowGap) : 0.0f;
	const float boxW  = contentW + padX * 2.0f;
	const float boxH  = padY + titleLineH + titleGap + rowsH + actionGap + skipBtnSize.y + padY;

	const float cx = static_cast<float>(SCENE_WIDTH)  / 2.0f;   // 550
	const float cy = static_cast<float>(SCENE_HEIGHT) / 2.0f;   // 300
	const float boxLeft  = cx - boxW / 2.0f;
	const float boxTop   = cy - boxH / 2.0f;
	const float boxRight = cx + boxW / 2.0f;

	// 纯色面板：尺寸由内容自动决定，面板矩形与文字坐标严格对齐，
	// 避免墓碑纹理花边内缩导致文字溢出可视边框
	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxW, boxH);

	// 标题（顶部居中）
	builder.Text(Vector(cx - titleW / 2.0f, boxTop + padY), static_cast<float>(titleFont), title, titleColor);

	// 配对行：绿=植物增益、红=僵尸增难，右侧「选择」按钮
	const float rowsTop = boxTop + padY + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		const float blockTop = rowsTop + i * (rowBlockH + rowGap);
		builder.Text(Vector(boxLeft + padX, blockTop),         static_cast<float>(rowFont), rows[i].plant,  green);
		builder.Text(Vector(boxLeft + padX, blockTop + lineH), static_cast<float>(rowFont), rows[i].zombie, red);

		const float btnY = blockTop + (rowBlockH - selectBtnSize.y) / 2.0f;
		builder.Button(u8"选择", Vector(boxRight - padX - selectBtnSize.x, btnY), selectBtnSize, 16,
			[this, i]() { this->ApplyPerkSelection(i); }, ResourceKeys::Textures::IMAGE_BUTTONSMALL, false);
	}

	// 两次选择共享同一轮的刷新额度；刷新只重抽当前候选，不结算当前选择机会。
	const bool canRefresh = mSurvivalPerkRefreshesRemaining > 0;
	const std::string refreshText = canRefresh
		? std::string(u8"刷新（剩余 ") + std::to_string(mSurvivalPerkRefreshesRemaining) + u8" 次）"
		: std::string(u8"刷新（已用完）");
	const float actionsLeft = cx - actionButtonsW / 2.0f;
	const float actionsY = boxTop + boxH - padY - skipBtnSize.y;
	builder.Button(refreshText, Vector(actionsLeft, actionsY), refreshBtnSize, 18,
		[this]() { this->RefreshSurvivalPerkSelection(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, false, canRefresh);

	// 放弃只消耗当前选择机会；第 1 次放弃后仍会进入第 2 次并保留刷新余额。
	builder.Button(u8"放弃本次", Vector(actionsLeft + refreshBtnSize.x + buttonGap, actionsY),
		skipBtnSize, 20, [this]() { this->ApplyPerkSelection(-1); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, false);

	mPerkSelectBox = builder.Show();
}

bool GameScene::RefreshSurvivalPerkSelection()
{
	if (!mBoard || !mSurvivalPerkSelectActive || mSurvivalPerkRefreshesRemaining <= 0)
		return false;

	// 先扣额度再重建，保证新框立即展示本轮准确的剩余次数。
	--mSurvivalPerkRefreshesRemaining;
	CloseSurvivalPerkSelectBox();
	RenderSurvivalPerkSelectStep();
	return true;
}

void GameScene::ApplyPerkSelection(int index)
{
	const bool picked = mBoard && index >= 0 && index < static_cast<int>(mCurrentPerkOffer.size());
	if (picked) {
		const PerkPairing& pr = mCurrentPerkOffer[index];
		auto& pm = mBoard->GetPerkManager();
		pm.AddPerk(pr.plant);
		pm.AddPerk(pr.zombie);
		++mSurvivalPerkPicksCompleted;
	}
	// 选择与放弃都会消耗当前机会；步骤进度不能再由实际获得的词条数推导。
	++mSurvivalPerkStepsCompleted;

	// 先失活可避免延迟销毁期间与下一步新框重叠一帧；真实按钮与 AutoTest 共用此生命周期。
	CloseSurvivalPerkSelectBox();

	if (mSurvivalPerkStepsCompleted < SURVIVAL_PERK_PICKS_PER_ROUND) {
		RenderSurvivalPerkSelectStep();
		return;
	}

	mSurvivalPerkSelectActive = false;
	mCurrentPerkOffer.clear();
	DeltaTime::SetPaused(false);

	// 两次机会均已结算，最多两对词条已写入 manager；选卡子流程会延后保存最终层数。
	BeginSurvivalCardSelect();
}

void GameScene::OpenPerkView()
{
	if (!mBoard) return;
	if (DeltaTime::IsPaused()) return;
	// 三向守卫：暂停菜单 / 轮间选词条模态 / 自身已开，均不叠开
	if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive) return;

	mPerkViewActive = true;
	DeltaTime::SetPaused(true);
	mPerkViewPage = 0;
	RenderPerkViewPage();
}

void GameScene::RenderPerkViewPage()
{
	if (!mBoard) return;
	auto& pm = mBoard->GetPerkManager();

	// 与 DrawText 同字体量逻辑像素宽（取不到字体时按半宽兜底）
	auto measureW = [](const std::string& s, int fontSize) -> float {
		TTF_Font* f = ResourceManager::GetInstance().GetFont(ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		if (!f) return static_cast<float>(s.size()) * fontSize * 0.5f;
		int w = 0, h = 0;
		TTF_SizeUTF8(f, s.c_str(), &w, &h);
		return static_cast<float>(w);
	};

	const glm::vec4 green{ 53, 191, 61, 255 };
	const glm::vec4 red  { 200, 60, 60, 255 };
	const glm::vec4 titleColor{ 245, 214, 127, 255 };

	// 收集已选词条（stacks>0），按 enum 顺序；descZh 已自带效果描述
	struct Line { std::string text; glm::vec4 color; };
	std::vector<Line> perkLines;
	int distinct = 0, total = 0;
	for (int i = 0; i < static_cast<int>(PerkType::COUNT); ++i) {
		PerkType t = static_cast<PerkType>(i);
		int n = pm.GetStacks(t);
		if (n <= 0) continue;
		const PerkInfo& info = SurvivalPerkManager::GetInfo(t);
		++distinct;
		total += n;
		Line ln;
		ln.text  = std::string(u8"· ") + info.descZh + u8"（已选 " + std::to_string(n) + u8" 次）";
		ln.color = (info.category == PerkCategory::PLANT_BUFF) ? green : red;
		perkLines.push_back(ln);
	}

	// 分页：每页最多 6 个 distinct 词条（distinct == perkLines.size()）
	constexpr int kPerksPerPage = 6;
	const int totalPages = (distinct > 0) ? ((distinct + kPerksPerPage - 1) / kPerksPerPage) : 1;
	if (mPerkViewPage < 0) mPerkViewPage = 0;
	if (mPerkViewPage > totalPages - 1) mPerkViewPage = totalPages - 1;
	const int pageStart = mPerkViewPage * kPerksPerPage;
	const int pageEnd   = std::min(distinct, pageStart + kPerksPerPage);

	std::string title = (distinct > 0)
		? (std::string(u8"已强化：") + std::to_string(distinct) + u8" 种词条 · 累计 " + std::to_string(total) + u8" 层")
		: std::string(u8"尚未选择任何强化词条");
	if (totalPages > 1)
		title += std::string(u8"（第 ") + std::to_string(mPerkViewPage + 1) + u8"/" + std::to_string(totalPages) + u8" 页）";

	// 固定面板（逻辑像素，居中于 550,300）
	const float boxW = 560.0f, boxH = 420.0f;
	const float padX = 30.0f, padY = 26.0f;
	const Vector closeBtnSize(160.0f, 44.0f);
	const float closeGap = 18.0f;
	const float cx = static_cast<float>(SCENE_WIDTH)  / 2.0f;
	const float cy = static_cast<float>(SCENE_HEIGHT) / 2.0f;
	const float boxLeft = cx - boxW / 2.0f;
	const float boxTop  = cy - boxH / 2.0f;
	const float innerW  = boxW - 2.0f * padX;
	const float availH  = boxH - 2.0f * padY - closeGap - closeBtnSize.y;
	const int   N       = pageEnd - pageStart;   // 本页行数（≤6）

	// 字号自动缩放：rowFont 18→10，titleFont=rowFont+4，挑「最大且能塞进固定面板」者；
	// 一路不满足则落到 floor=10（容忍轻微挤压，仍优于溢出可视边框）
	int   rowFont = 10, titleFont = 14;
	float titleLineH = 0.0f, rowLineH = 0.0f, titleGap = 0.0f, rowGap = 0.0f, contentH = 0.0f;
	for (int fnt = 18; fnt >= 10; --fnt) {
		const int   tf  = fnt + 4;
		const float tlh = tf  * 1.4f;
		const float rlh = fnt * 1.4f;
		const float tg  = fnt * 0.9f;
		const float rg  = fnt * 0.5f;
		const float ch  = tlh + (N > 0 ? (tg + N * rlh + (N - 1) * rg) : 0.0f);
		float maxW = measureW(title, tf);
		for (int li = pageStart; li < pageEnd; ++li) {
			float w = measureW(perkLines[li].text, fnt);
			if (w > maxW) maxW = w;
		}
		const bool fits = (ch <= availH) && (maxW <= innerW);
		if (fits || fnt == 10) {
			rowFont = fnt; titleFont = tf;
			titleLineH = tlh; rowLineH = rlh; titleGap = tg; rowGap = rg; contentH = ch;
			if (fits) break;   // 否则 fnt==12 兜底，循环自然结束
		}
	}

	// 纯色面板，同选词条框，规避墓碑花边内缩导致文字溢出
	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxW, boxH);

	// 内容块在 availH 区域内垂直居中，词条少时不孤悬顶部
	const float blockTop = boxTop + padY + (availH - contentH) / 2.0f;
	const float titleW   = measureW(title, titleFont);
	builder.Text(Vector(cx - titleW / 2.0f, blockTop), static_cast<float>(titleFont), title, titleColor);

	float y = blockTop + titleLineH + titleGap;
	for (int i = 0; i < N; ++i) {
		const Line& ln = perkLines[pageStart + i];
		builder.Text(Vector(boxLeft + padX, y), static_cast<float>(rowFont), ln.text, ln.color);
		y += rowLineH + rowGap;
	}

	// 底部一行三按钮：上一页（左·仅非首页）· 关闭（中·恒显）· 下一页（右·仅有下一页）
	const float    boxRight   = cx + boxW / 2.0f;
	const Vector   navBtnSize(110.0f, 44.0f);
	const float    btnY       = boxTop + boxH - padY - closeBtnSize.y;

	builder.Button(u8"关闭", Vector(cx - closeBtnSize.x / 2.0f, btnY), closeBtnSize, 20,
		[this]() { this->ClosePerkView(); }, ResourceKeys::Textures::IMAGE_BUTTONBIG);
	if (mPerkViewPage > 0)
		builder.Button(u8"上一页", Vector(boxLeft + padX, btnY), navBtnSize, 18,
			[this]() { --mPerkViewPage; RenderPerkViewPage(); });
	if (mPerkViewPage < totalPages - 1)
		builder.Button(u8"下一页", Vector(boxRight - padX - navBtnSize.x, btnY), navBtnSize, 18,
			[this]() { ++mPerkViewPage; RenderPerkViewPage(); });

	mPerkViewBox = builder.Show();
}

void GameScene::ClosePerkView()
{
	mPerkViewActive = false;
	DeltaTime::SetPaused(false);
	// 查看面板仍由「关闭」按钮 autoClose=true 在帧末安全销毁；词条选择框则由 ApplyPerkSelection 统一关闭。
}

void GameScene::BeginSurvivalCardSelect()
{
	mSurvivalRoundTransition = true;

	// 让上一轮已升起的旗子平滑降回（否则会一直悬着，直到下一轮第 1 波 SetupFlags 才突变重置）。
	// 滑块归位由 GameProgress::Update 依据 mCurrentWave(已被 OnSurvivalRoundClear 归 0)自动完成。
	if (mGameProgress) mGameProgress->LowerAllFlags(1.0f);

	// 冷却快照已在进入词条页时提前完成；现在清空上一轮卡槽，让下一轮从空槽重新选择。
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

	GameMessageBox::Builder(Vector(SCENE_WIDTH / 2, SCENE_HEIGHT / 2))
		.Title(u8"游戏结束")
		.Message(u8"僵尸吃掉了你的脑子！")
		.Scale(1.5f)
		.Button(u8"返回菜单", Vector(380, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToBackMenu = true;
			DeltaTime::SetPaused(false);
		})
		.Button(u8"重新开始", Vector(560, 380), Vector(125 * 0.8f, 52 * 0.8f), 14, [this]() {
			this->mReadyToRestart = true;
			DeltaTime::SetPaused(false);
		})
		.Show();
}

// ============ 开发者模式（-develop，D 键面板） ============

void GameScene::OpenDevPanel()
{
	if (!GameAPP::mDevelopMode || !mBoard) return;
	if (DeltaTime::IsPaused()) return;
	// 多向守卫：暂停菜单 / 词条选择 / 词条查看 / 自身已开，均不叠开
	if (mOpenMenu || mSurvivalPerkSelectActive || mPerkViewActive || mDevPanelActive) return;

	mDevPanelActive = true;
	DeltaTime::SetPaused(true);
	RenderDevPanel();
}

void GameScene::CloseDevPanel()
{
	if (auto box = mDevPanelBox.lock())
		GameObjectManager::GetInstance().DestroyGameObject(box);
	mDevPanelBox.reset();
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
}

void GameScene::RenderDevPanel()
{
	// 状态变化即整体重建：旧盒由被点按钮 autoClose=true 帧末自毁，这里只管建新盒
	const float cx = static_cast<float>(SCENE_WIDTH) / 2.0f;    // 550
	const float cy = static_cast<float>(SCENE_HEIGHT) / 2.0f;   // 300
	const Vector boxSize(520.0f, 400.0f);
	const glm::vec4 titleColor{ 245, 214, 127, 255 };
	const glm::vec4 textColor { 230, 230, 230, 255 };

	GameMessageBox::Builder builder{ Vector(cx, cy) };
	builder.Panel(boxSize.x, boxSize.y);
	builder.Text(Vector(cx - 70.0f, 110.0f), 22.0f, u8"开发者面板", titleColor);

	auto toggleText = [](const char* name, bool on) {
		return std::string(name) + (on ? u8"：开" : u8"：关");
	};

	// 作弊开关（点击翻转后重建面板刷新文字）
	builder.Button(toggleText(u8"无冷却种植", GameAPP::mDevNoCooldown),
		Vector(340.0f, 160.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevNoCooldown = !GameAPP::mDevNoCooldown; RenderDevPanel(); });
	builder.Button(toggleText(u8"无视阳光", GameAPP::mDevFreePlant),
		Vector(340.0f, 206.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevFreePlant = !GameAPP::mDevFreePlant; RenderDevPanel(); });
	builder.Button(toggleText(u8"暂停刷怪", GameAPP::mDevSpawnPaused),
		Vector(580.0f, 160.0f), Vector(200.0f, 36.0f), 16,
		[this]() { GameAPP::mDevSpawnPaused = !GameAPP::mDevSpawnPaused; RenderDevPanel(); });

	// 僵尸类型选择行
	const int zn = static_cast<int>(kDevZombieTable.size());
	builder.Button("<", Vector(340.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + zn - 1) % zn; RenderDevPanel(); });
	builder.Text(Vector(395.0f, 260.0f), 14.0f,
		kDevZombieTable[mDevZombieIndex].second, textColor);
	builder.Button(">", Vector(560.0f, 252.0f), Vector(40.0f, 36.0f), 16,
		[this, zn]() { mDevZombieIndex = (mDevZombieIndex + 1) % zn; RenderDevPanel(); });
	builder.Button(u8"召唤", Vector(620.0f, 252.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->BeginDevSpawnMode(); });

	// 关卡选择行
	builder.Button(u8"-", Vector(340.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { if (mDevLevelSel > 1) --mDevLevelSel; RenderDevPanel(); });
	builder.Text(Vector(420.0f, 310.0f), 16.0f,
		std::string(u8"关卡 ") + std::to_string(mDevLevelSel), textColor);
	builder.Button(u8"+", Vector(560.0f, 302.0f), Vector(40.0f, 36.0f), 16,
		[this]() { ++mDevLevelSel; RenderDevPanel(); });
	builder.Button(u8"进入", Vector(620.0f, 302.0f), Vector(90.0f, 36.0f), 16,
		[this]() { this->DevJumpToLevel(); });
	builder.Button(u8"无尽1000", Vector(340.0f, 348.0f), Vector(110.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1000; RenderDevPanel(); });
	builder.Button(u8"夜无尽1001", Vector(460.0f, 348.0f), Vector(130.0f, 32.0f), 14,
		[this]() { mDevLevelSel = 1001; RenderDevPanel(); });

	// 底部：下一波 / 关闭
	builder.Button(u8"下一波", Vector(360.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { this->DevTriggerNextWave(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG, false);   // 不自动关面板，可连点
	builder.Button(u8"关闭", Vector(600.0f, 420.0f), Vector(120.0f, 40.0f), 18,
		[this]() { mDevPanelActive = false; DeltaTime::SetPaused(false); mDevPanelBox.reset(); },
		ResourceKeys::Textures::IMAGE_BUTTONBIG);

	mDevPanelBox = builder.Show();
}

void GameScene::BeginDevSpawnMode()
{
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
	mDevPanelBox.reset();          // 盒子由按钮 autoClose 帧末自毁
	mDevSpawnMode = true;
	if (mCardSlotManager) mCardSlotManager->DeselectCard();   // 防手持卡与召唤点击叠加种植

	// 顶部提示（一次注册，靠 mDevSpawnMode 守卫显隐）
	if (!mDevHintRegistered) {
		RegisterDrawCommand("DevSpawnHint",
			[this](Graphics* g) {
				if (!mDevSpawnMode) return;
				const std::string tip = std::string(u8"召唤模式：")
					+ kDevZombieTable[mDevZombieIndex].second + u8"（左键放置，ESC 退出，RSHIFT 回面板）";
				GameAPP::GetInstance().DrawText(tip,
					g->LogicalToWorld(300, 30), { 255, 90, 90, 255 },
					ResourceKeys::Fonts::FONT_FZCQ, 18);
			},
			LAYER_UI + 100000);
		SortDrawCommands();
		mDevHintRegistered = true;
	}
}
void GameScene::DevJumpToLevel()
{
	// 不能在按钮回调（本帧 Update 中段）直接 SwitchTo 销毁自身——
	// 与 mReadyToBackMenu 同理，置 pending 由 Update 尾部统一执行
	mDevPanelActive = false;
	DeltaTime::SetPaused(false);
	mDevPanelBox.reset();
	mDevPendingLevel = mDevLevelSel;
}

void GameScene::DevTriggerNextWave()
{
	// 面板不关闭（按钮 autoClose=false）：直接走出波入口，暂停中也立即生成，连点连出多波
	if (mBoard && mBoard->mBoardState == BoardState::GAME
		&& mBoard->mCurrentWave < mBoard->mMaxWave) {
		mBoard->SummonNextWave();
	}
}

void GameScene::ShowScreenFlash(float duration, float peakAlpha)
{
	if (duration <= 0.0f) return;
	mScreenFlashDuration = duration;
	mScreenFlashTimer = duration;
	mScreenFlashPeakAlpha = std::clamp(peakAlpha, 0.0f, 255.0f);
}

void GameScene::ShowWeatherForecastFailure(RainIntensity forecast, RainIntensity actual)
{
	mFailedForecastRainIntensity = forecast;
	mActualForecastRainIntensity = actual;
	mWeatherForecastFailureTimer = kForecastFailureDuration;
}

void GameScene::ShowCurrentWeatherNotice()
{
	mCurrentWeatherNoticeTimer = kCurrentWeatherNoticeDuration;
}

void GameScene::RestoreCurrentWeatherNotice(float remaining)
{
	mCurrentWeatherNoticeTimer = std::clamp(remaining, 0.0f,
		kCurrentWeatherNoticeDuration);
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
