#include "TestDriver.h"
#include "../../GameAPP.h"
#include "../../Renderer/VulkanRenderer.h"
#include "../../DeltaTime.h"
#include "../../Logger.h"
#include "../SceneManager.h"
#include "../GameScene.h"
#include "../ChooseCardUI.h"
#include "../Board.h"
#include "../LawnMower.h"
#include "../AudioSystem.h"
#include "../Card.h"
#include "../CardComponent.h"
#include "../Plant/PlantType.h"
#include "../Plant/Plant.h"
#include "../Plant/LilyPad.h"
#include "../Plant/Shooter.h"
#include "../Bullet/Bullet.h"
#include "../Zombie/ZombieType.h"
#include "../Zombie/Zombie.h"
#include "../Zombie/ZombieCharred.h"
#include "../Zombie/EliteDancerZombie.h"
#include "../Zombie/PoolNormalZombie.h"
#include "../Trophy.h"   // dump_state 输出奖杯坐标
#include "../Crater.h"   // dump_state 输出毁灭菇弹坑
#include "../../Reanimation/Animator.h"   // dump_state 查询轨道可见性（如铁门僵尸手臂）
#include <filesystem>
#include <algorithm>
#include <cmath>
#include <unordered_map>
#include <SDL2/SDL.h>

namespace {
#define PT(n) { #n, PlantType::n }
	const std::unordered_map<std::string, PlantType> kPlantNames = {
		PT(PLANT_PEASHOOTER), PT(PLANT_SUNFLOWER), PT(PLANT_CHERRYBOMB), PT(PLANT_WALLNUT),
		PT(PLANT_POTATOMINE), PT(PLANT_SNOWPEA), PT(PLANT_CHOMPER), PT(PLANT_REPEATER),
		PT(PLANT_PUFFSHROOM), PT(PLANT_SUNSHROOM), PT(PLANT_FUMESHROOM),
		PT(PLANT_HYPNOSHROOM), PT(PLANT_SCAREDYSHROOM), PT(PLANT_ICESHROOM), PT(PLANT_ICEFUMESHROOM),
		PT(PLANT_DOOMSHROOM),
		PT(PLANT_LILYPAD), PT(PLANT_SQUASH), PT(PLANT_THREEPEATER), PT(PLANT_TANGLEKELP),
		PT(PLANT_JALAPENO), PT(PLANT_SPIKEWEED), PT(PLANT_TORCHWOOD), PT(PLANT_TALLNUT),
		PT(PLANT_SEASHROOM), PT(PLANT_PLANTERN), PT(PLANT_CACTUS), PT(PLANT_BLOVER),
		PT(PLANT_SPLITPEA), PT(PLANT_STARFRUIT), PT(PLANT_PUMPKINSHELL), PT(PLANT_MAGNETSHROOM),
		PT(PLANT_CABBAGEPULT), PT(PLANT_FLOWERPOT), PT(PLANT_KERNELPULT), PT(PLANT_INSTANT_COFFEE),
		PT(PLANT_GARLIC), PT(PLANT_UMBRELLA), PT(PLANT_MARIGOLD), PT(PLANT_MELONPULT),
		PT(PLANT_GATLINGPEA), PT(PLANT_TWINSUNFLOWER), PT(PLANT_GLOOMSHROOM), PT(PLANT_CATTAIL),
		PT(PLANT_WINTERMELON), PT(PLANT_GOLD_MAGNET), PT(PLANT_SPIKEROCK), PT(PLANT_COBCANNON),
		PT(PLANT_IMITATER), PT(PLANT_EXPLODE_O_NUT), PT(PLANT_GIANT_WALLNUT), PT(PLANT_SPROUT),
		PT(PLANT_LEFTPEATER),
	};
#undef PT
#define BT(n) { #n, BulletType::n }
	const std::unordered_map<std::string, BulletType> kBulletNames = {
		BT(BULLET_PEA), BT(BULLET_SNOWPEA), BT(BULLET_PUFF),
	};
#undef BT
#define ZT(n) { #n, ZombieType::n }
	const std::unordered_map<std::string, ZombieType> kZombieNames = {
		ZT(ZOMBIE_NORMAL), ZT(ZOMBIE_TRAFFIC_CONE), ZT(ZOMBIE_POLEVAULTER), ZT(ZOMBIE_BUCKET),
		ZT(ZOMBIE_FASTBUCKET), ZT(ZOMBIE_NEWSPAPER), ZT(ZOMBIE_FASTPAPER), ZT(ZOMBIE_DOOR),
		ZT(ZOMBIE_FOOTBALL), ZT(ZOMBIE_DANCER), ZT(ZOMBIE_BACKUP_DANCER), ZT(ZOMBIE_ELITE_DANCER), ZT(ZOMBIE_PINK_FOOTBALL),
		ZT(ZOMBIE_REINFORCED_DOOR),
		ZT(ZOMBIE_POOL_NORMAL), ZT(ZOMBIE_POOL_CONE), ZT(ZOMBIE_POOL_BUCKET),
		ZT(ZOMBIE_DUCKY_TUBE),
		ZT(ZOMBIE_SNORKEL), ZT(ZOMBIE_ZAMBONI), ZT(ZOMBIE_BOBSLED), ZT(ZOMBIE_DOLPHIN_RIDER),
		ZT(ZOMBIE_JACK_IN_THE_BOX), ZT(ZOMBIE_BALLOON), ZT(ZOMBIE_DIGGER), ZT(ZOMBIE_POGO),
		ZT(ZOMBIE_YETI), ZT(ZOMBIE_BUNGEE), ZT(ZOMBIE_LADDER), ZT(ZOMBIE_CATAPULT),
		ZT(ZOMBIE_GARGANTUAR), ZT(ZOMBIE_IMP), ZT(ZOMBIE_BOSS), ZT(ZOMBIE_PEA_HEAD),
		ZT(ZOMBIE_WALLNUT_HEAD), ZT(ZOMBIE_JALAPENO_HEAD), ZT(ZOMBIE_GATLING_HEAD),
		ZT(ZOMBIE_SQUASH_HEAD), ZT(ZOMBIE_TALLNUT_HEAD), ZT(ZOMBIE_REDEYE_GARGANTUAR),
	};
#undef ZT
#define PK(n) { #n, PerkType::n }
	const std::unordered_map<std::string, PerkType> kPerkNames = {
		PK(PLANT_DAMAGE_UP), PK(ZOMBIE_HEALTH_UP), PK(ZOMBIE_DAMAGE_RESIST),
		PK(ZOMBIE_DAMAGE_UP), PK(ZOMBIE_INVULN_HITS), PK(PLANT_REGEN),
		PK(PLANT_ATTACK_SPEED), PK(PLANT_DAMAGE_REDUCTION), PK(PLANT_SUN_BONUS),
		PK(PLANT_CARD_RECHARGE),
	};
#undef PK
	const std::unordered_map<std::string, BoardState> kBoardStateNames = {
		{ "CHOOSE_CARD", BoardState::CHOOSE_CARD }, { "GAME", BoardState::GAME },
		{ "LOSE_GAME", BoardState::LOSE_GAME }, { "WIN", BoardState::WIN },
		{ "NONE", BoardState::NONE },
	};
	const std::unordered_map<std::string, DamageSource> kDamageSourceNames = {
		{ "PLANT", DamageSource::PLANT }, { "ZOMBIE", DamageSource::ZOMBIE },
		{ "OTHER", DamageSource::OTHER },
	};
	const std::unordered_map<std::string, RainIntensity> kRainIntensityNames = {
		{ "CLEAR", RainIntensity::CLEAR }, { "LIGHT", RainIntensity::LIGHT },
		{ "MEDIUM", RainIntensity::MEDIUM }, { "HEAVY", RainIntensity::HEAVY },
	};
	const std::unordered_map<std::string, TyphoonStrength> kTyphoonStrengthNames = {
		{ "NONE", TyphoonStrength::NONE }, { "TYPHOON", TyphoonStrength::TYPHOON },
		{ "SEVERE", TyphoonStrength::SEVERE }, { "SUPER", TyphoonStrength::SUPER },
	};
	const std::unordered_map<std::string, WindDirection> kWindDirectionNames = {
		{ "NONE", WindDirection::NONE }, { "HOUSE", WindDirection::TOWARD_HOUSE },
		{ "FRONT", WindDirection::TOWARD_FRONT },
	};

	const std::unordered_map<std::string, Uint8> kMouseButtonNames = {
		{ "left", SDL_BUTTON_LEFT }, { "right", SDL_BUTTON_RIGHT },
		{ "middle", SDL_BUTTON_MIDDLE },
	};

	// 按键名 → SDL_Keycode。初始覆盖常用集，按需加行（与植物/僵尸名表同惯例）。
	const std::unordered_map<std::string, SDL_Keycode> kKeyNames = {
		{ "a", SDLK_a }, { "b", SDLK_b }, { "c", SDLK_c }, { "d", SDLK_d },
		{ "e", SDLK_e }, { "f", SDLK_f }, { "g", SDLK_g }, { "h", SDLK_h },
		{ "i", SDLK_i }, { "j", SDLK_j }, { "k", SDLK_k }, { "l", SDLK_l },
		{ "m", SDLK_m }, { "n", SDLK_n }, { "o", SDLK_o }, { "p", SDLK_p },
		{ "q", SDLK_q }, { "r", SDLK_r }, { "s", SDLK_s }, { "t", SDLK_t },
		{ "u", SDLK_u }, { "v", SDLK_v }, { "w", SDLK_w }, { "x", SDLK_x },
		{ "y", SDLK_y }, { "z", SDLK_z },
		{ "0", SDLK_0 }, { "1", SDLK_1 }, { "2", SDLK_2 }, { "3", SDLK_3 },
		{ "4", SDLK_4 }, { "5", SDLK_5 }, { "6", SDLK_6 }, { "7", SDLK_7 },
		{ "8", SDLK_8 }, { "9", SDLK_9 },
		{ "space", SDLK_SPACE }, { "enter", SDLK_RETURN }, { "return", SDLK_RETURN },
		{ "escape", SDLK_ESCAPE }, { "esc", SDLK_ESCAPE }, { "tab", SDLK_TAB },
		{ "backspace", SDLK_BACKSPACE }, { "rshift", SDLK_RSHIFT },
		// 方向键：这里的 "left"/"right" 是键盘方向键，与 kMouseButtonNames 的 "left"/"right"
		// 是各自独立的表（key 命令查此表，click 命令查鼠标表），不冲突。
		{ "up", SDLK_UP }, { "down", SDLK_DOWN }, { "left", SDLK_LEFT }, { "right", SDLK_RIGHT },
		{ "f1", SDLK_F1 }, { "f2", SDLK_F2 }, { "f3", SDLK_F3 }, { "f4", SDLK_F4 },
		{ "f5", SDLK_F5 }, { "f6", SDLK_F6 }, { "f7", SDLK_F7 }, { "f8", SDLK_F8 },
		{ "f9", SDLK_F9 }, { "f10", SDLK_F10 }, { "f11", SDLK_F11 }, { "f12", SDLK_F12 },
	};

	std::string PlantTypeName(PlantType t) {
		for (const auto& [k, v] : kPlantNames) if (v == t) return k;
		return "UNKNOWN_PLANT_" + std::to_string(static_cast<int>(t));
	}
	std::string ZombieTypeName(ZombieType t) {
		for (const auto& [k, v] : kZombieNames) if (v == t) return k;
		return "UNKNOWN_ZOMBIE_" + std::to_string(static_cast<int>(t));
	}
	std::string BoardStateName(BoardState s) {
		for (const auto& [k, v] : kBoardStateNames) if (v == s) return k;
		return "UNKNOWN";
	}
	std::string RainIntensityName(RainIntensity intensity) {
		for (const auto& [k, v] : kRainIntensityNames) if (v == intensity) return k;
		return "UNKNOWN";
	}
	std::string TyphoonStrengthName(TyphoonStrength strength) {
		for (const auto& [k, v] : kTyphoonStrengthNames) if (v == strength) return k;
		return "UNKNOWN";
	}
	std::string WindDirectionName(WindDirection direction) {
		for (const auto& [k, v] : kWindDirectionNames) if (v == direction) return k;
		return "UNKNOWN";
	}
	std::string PlayStateName(PlayState s) {
		switch (s) {
		case PlayState::PLAY_NONE:    return "PLAY_NONE";
		case PlayState::PLAY_REPEAT:  return "PLAY_REPEAT";
		case PlayState::PLAY_ONCE:    return "PLAY_ONCE";
		case PlayState::PLAY_ONCE_TO: return "PLAY_ONCE_TO";
		}
		return "UNKNOWN";
	}
	std::string BackgroundName(Background background) {
		switch (background) {
		case Background::GROUND_DAY:       return "GROUND_DAY";
		case Background::GROUND_NIGHT:     return "GROUND_NIGHT";
		case Background::WATER_POOL:       return "WATER_POOL";
		case Background::NIGHT_WATER_POOL: return "NIGHT_WATER_POOL";
		case Background::ROOF:             return "ROOF";
		case Background::NIGHT_ROOF:       return "NIGHT_ROOF";
		}
		return "UNKNOWN";
	}
	const char* MowerTypeName(MowerType type) {
		switch (type) {
		case MowerType::LAWN:  return "LAWN";
		case MowerType::WATER: return "WATER";
		case MowerType::ROOF:  return "ROOF";
		}
		return "UNKNOWN";
	}
	const char* MowerHeightName(MowerHeight height) {
		switch (height) {
		case MowerHeight::LAND:     return "LAND";
		case MowerHeight::ENTERING: return "ENTERING";
		case MowerHeight::IN_POOL:  return "IN_POOL";
		case MowerHeight::EXITING:  return "EXITING";
		}
		return "UNKNOWN";
	}

	GameScene* CurrentGameScene() {
		return dynamic_cast<GameScene*>(SceneManager::GetInstance().GetCurrentScene());
	}
}

TestDriver& TestDriver::GetInstance() {
	static TestDriver instance;
	return instance;
}

bool TestDriver::LoadScript(const std::string& path) {
	std::ifstream f(path);
	if (!f) {
		LOG_ERROR("AutoTest") << "无法打开脚本: " << path;
		return false;
	}
	nlohmann::json j;
	try {
		f >> j;
	}
	catch (const std::exception& e) {
		LOG_ERROR("AutoTest") << "脚本 JSON 解析失败: " << e.what();
		return false;
	}
	if (!j.contains("commands") || !j["commands"].is_array() || j["commands"].empty()) {
		LOG_ERROR("AutoTest") << "脚本缺少非空 commands 数组";
		return false;
	}
	for (const auto& c : j["commands"]) mCommands.push_back(c);

	mOutDir = (std::filesystem::path("./autotest/out") /
		std::filesystem::path(path).stem()).string();
	std::error_code ec;
	std::filesystem::create_directories(mOutDir, ec);
	if (ec) {
		LOG_ERROR("AutoTest") << "无法创建输出目录 " << mOutDir << ": " << ec.message();
		return false;
	}
	mRunLog.open(mOutDir + "/run.log", std::ios::trunc);
	if (!mRunLog.is_open()) {
		LOG_ERROR("AutoTest") << "无法创建 " << mOutDir << "/run.log（权威日志，缺失即盲跑）";
		return false;
	}

	mActive = true;
	Log("script loaded: " + path + " (" + std::to_string(mCommands.size()) + " commands)");
	if (GameAPP::mAutoTestLoadSave)
		Log("level save loading enabled (read-only): ./saves");
	return true;
}

void TestDriver::Log(const std::string& msg) {
	if (mRunLog.is_open()) {
		mRunLog << "[f" << mFrame << "] " << msg << "\n";
		mRunLog.flush();
	}
	LOG_INFO("AutoTest") << msg;   // Release 编译期裁掉，run.log 才是权威记录
}

void TestDriver::Fail(const std::string& reason) {
	const std::string op = (mIndex < mCommands.size())
		? mCommands[mIndex].value("op", "?") : "?";
	Log("FAIL at cmd#" + std::to_string(mIndex) + " (" + op + "): " + reason);
	LOG_ERROR("AutoTest") << "FAIL at cmd#" << mIndex << " (" << op << "): " << reason;
	mExitCode = 1;
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Finish() {
	Log("script finished OK");
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Update() {
	if (!mActive) return;
	++mFrame;
	mBreakFrame = false;
	int guard = 0;
	while (mActive && !mBreakFrame && mIndex < mCommands.size()) {
		if (!ExecuteCurrent()) break;          // 等待中，下帧重试
		Log("done cmd#" + std::to_string(mIndex) + " (" +
			mCommands[mIndex].value("op", "?") + ")");
		++mIndex;
		mWaitAccum = 0.0f;
		mFramesLeft = -1;
		mTimeoutAccum = 0.0f;
		mInputPhase = -1;
		if (++guard > 64) break;               // 单帧推进上限，防脚本自旋
	}
	if (mActive && mIndex >= mCommands.size()) Finish();
}

bool TestDriver::ExecuteCurrent() {
	const auto& cmd = mCommands[mIndex];
	const std::string op = cmd.value("op", "");

	// 等待型命令的超时看门狗（墙钟语义，不受 timescale 影响）
	mTimeoutAccum += DeltaTime::GetUnscaledDeltaTime();
	const float timeout = cmd.value("timeout", 15.0f);
	if (mTimeoutAccum > timeout) {
		Fail("timeout (" + std::to_string(timeout) + "s)");
		return false;
	}

	if (op == "wait_seconds") {
		mWaitAccum += DeltaTime::GetDeltaTime();
		return mWaitAccum >= cmd.value("value", 0.0f);
	}
	if (op == "wait_frames") {
		if (mFramesLeft < 0) mFramesLeft = cmd.value("value", 0);
		if (mFramesLeft == 0) return true;   // value=0 或已数完：立即完成
		--mFramesLeft;
		return mFramesLeft == 0;
	}
	if (op == "goto_level") {
		if (!cmd.contains("level")) { Fail("goto_level 缺 level 字段"); return false; }
		auto& sm = SceneManager::GetInstance();
		sm.SetGlobalData("EnterLevel", std::to_string(cmd["level"].get<int>()));
		if (!sm.SwitchTo("GameScene")) { Fail("SwitchTo(GameScene) 失败"); return false; }
		return true;
	}
	if (op == "set_timescale") {
		DeltaTime::SetTimeScale(cmd.value("value", 1.0f));
		return true;
	}
	if (op == "choose_cards") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->IsChooseCardReady()) return false;   // 等开场动画铺完卡
		ChooseCardUI* ui = gs->GetChooseCardUI();
		for (const auto& nameJson : cmd.value("cards", nlohmann::json::array())) {
			const std::string name = nameJson.get<std::string>();
			auto it = kPlantNames.find(name);
			if (it == kPlantNames.end()) { Fail("未知植物类型: " + name); return false; }
			Card* card = ui->FindCardByType(it->second);
			if (!card) {                       // 玩家未拥有该卡：AutoTest 直接补一张
				ui->AddCard(it->second);
				card = ui->FindCardByType(it->second);
			}
			if (!card) { Fail("选卡失败（AddCard 后仍找不到）: " + name); return false; }
			if (!ui->IsCardSelected(card) && !ui->ToggleCardSelection(card)) {
				Fail("选卡失败（超出选卡上限？）: " + name);
				return false;
			}
		}
		gs->ChooseCardComplete();
		return true;
	}
	if (op == "wait_state") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) return false;
		auto it = kBoardStateNames.find(cmd.value("state", ""));
		if (it == kBoardStateNames.end()) { Fail("未知 BoardState: " + cmd.value("state", "")); return false; }
		return gs->GetBoard()->mBoardState == it->second;
	}
	if (op == "set_sun") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("set_sun: 不在 GameScene 或 Board 为空"); return false; }
		gs->GetBoard()->mSun = std::min(cmd.value("value", 0), MAX_SUN);
		return true;
	}
	if (op == "set_weather") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("set_weather: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kRainIntensityNames.find(cmd.value("intensity", ""));
		if (it == kRainIntensityNames.end()) {
			Fail("set_weather: intensity 必须是 CLEAR/LIGHT/MEDIUM/HEAVY");
			return false;
		}
		gs->GetBoard()->SetRainForTesting(it->second, cmd.value("duration", 30.0f),
			cmd.value("canIntensify", false));
		if (gs->GetBoard()->GetRainIntensity() != it->second) {
			Fail("set_weather: 当前关卡不支持天气，天气未生效");
			return false;
		}
		return true;
	}
	if (op == "set_typhoon") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("set_typhoon: 不在 GameScene 或 Board 为空"); return false; }
		auto strengthIt = kTyphoonStrengthNames.find(cmd.value("strength", ""));
		auto directionIt = kWindDirectionNames.find(cmd.value("direction", "NONE"));
		if (strengthIt == kTyphoonStrengthNames.end() || directionIt == kWindDirectionNames.end()) {
			Fail("set_typhoon: strength 必须是 NONE/TYPHOON/SEVERE/SUPER，direction 必须是 NONE/HOUSE/FRONT");
			return false;
		}
		if (!gs->GetBoard()->SetTyphoonForTesting(strengthIt->second, directionIt->second,
			cmd.value("gustIn", 30.0f), cmd.value("directionIn", 30.0f),
			cmd.value("gustsRemaining", 1), cmd.value("decayIn", 30.0f))) {
			Fail("set_typhoon: 只有大雨允许设置台风，且启用台风时必须提供 HOUSE/FRONT 风向");
			return false;
		}
		return true;
	}
	if (op == "reroll_typhoon_direction") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("reroll_typhoon_direction: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->RerollWindDirectionForTesting(cmd.value("directionRoll", 0))) {
			Fail("reroll_typhoon_direction: 当前必须有台风，directionRoll 必须为 1..2");
			return false;
		}
		return true;
	}
	if (op == "trigger_typhoon_gust") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("trigger_typhoon_gust: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->TriggerTyphoonGustForTesting(cmd.value("plantMoveIn", 0.0f))) {
			Fail("trigger_typhoon_gust: 当前不是带台风的大雨");
			return false;
		}
		return true;
	}
	if (op == "roll_typhoon") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("roll_typhoon: 不在 GameScene 或 Board 为空"); return false; }
		auto directionIt = kWindDirectionNames.find(cmd.value("direction", ""));
		if (directionIt == kWindDirectionNames.end()
			|| !gs->GetBoard()->RollTyphoonForTesting(cmd.value("chanceRoll", 0),
				cmd.value("strengthRoll", 0), directionIt->second)) {
			Fail("roll_typhoon: 只允许大雨，chanceRoll 必须为 1..100，strengthRoll 必须落在当前权重总和内，direction 必须为 HOUSE/FRONT");
			return false;
		}
		return true;
	}
	if (op == "set_weather_forecast") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("set_weather_forecast: 不在 GameScene 或 Board 为空"); return false; }
		auto forecastIt = kRainIntensityNames.find(cmd.value("forecast", ""));
		auto actualIt = kRainIntensityNames.find(cmd.value("actual", ""));
		if (forecastIt == kRainIntensityNames.end() || actualIt == kRainIntensityNames.end()) {
			Fail("set_weather_forecast: forecast/actual 必须是 CLEAR/LIGHT/MEDIUM/HEAVY");
			return false;
		}
		if (!gs->GetBoard()->SetWeatherForecastForTesting(forecastIt->second, actualIt->second,
			cmd.value("revealIn", 1.0f))) {
			Fail("set_weather_forecast: 当前关卡不支持天气，或天气尚未初始化");
			return false;
		}
		return true;
	}
	if (op == "roll_weather_forecast") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("roll_weather_forecast: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->PrepareWeatherForecastForTesting(
			cmd.value("weatherRoll", 0), cmd.value("revealIn", 0.1f))) {
			Fail("roll_weather_forecast: 只允许已初始化的晴天，weatherRoll 必须落在当前权重总和内");
			return false;
		}
		return true;
	}
	if (op == "advance_weather_phase") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("advance_weather_phase: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->AdvanceRainPhaseForTesting(cmd.value("roll", 1))) {
			Fail("advance_weather_phase: 当前无雨，或 roll 超出当前转档总权重");
			return false;
		}
		return true;
	}
	if (op == "trigger_lightning") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("trigger_lightning: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->TriggerLightningForTesting()) {
			Fail("trigger_lightning: 只有大雨允许闪电");
			return false;
		}
		return true;
	}
	if (op == "set_adventure_level") {
		const int level = cmd.value("level", 1);
		if (level < 1) { Fail("set_adventure_level: level 必须为正数"); return false; }
		GameAPP::GetInstance().mAdventureLevel = level;
		return true;
	}
	if (op == "force_trophy") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("force_trophy: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		if (board->mBoardState != BoardState::GAME) { Fail("force_trophy: Board 尚未进入 GAME"); return false; }
		board->CreateTrophy(Vector(cmd.value("x", 500.0f), cmd.value("y", 300.0f)));
		return true;
	}
	if (op == "force_survival_round") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("force_survival_round: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		if (!board->mIsSurvival) { Fail("force_survival_round: 非生存模式关卡"); return false; }
		int round = cmd.value("round", 1);
		if (!board->SetSurvivalRoundForTesting(round)) {
			Fail("force_survival_round: round 必须为正数");
			return false;
		}
		return true;
	}
	if (op == "force_survival_round_clear") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("force_survival_round_clear: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		if (!board->mIsSurvival) { Fail("force_survival_round_clear: 非生存模式关卡"); return false; }
		if (board->mBoardState != BoardState::GAME) { Fail("force_survival_round_clear: Board 尚未进入 GAME"); return false; }
		// 走正式轮清入口，覆盖词条选择、选卡过场及场上对象保留行为。
		board->OnSurvivalRoundClear();
		return true;
	}
	if (op == "plant") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("plant: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kPlantNames.find(cmd.value("type", ""));
		if (it == kPlantNames.end()) { Fail("未知植物类型: " + cmd.value("type", "")); return false; }
		Plant* p = gs->GetBoard()->CreatePlant(it->second,
			cmd.value("row", 0), cmd.value("col", 0));
		if (!p) { Fail("CreatePlant 返回空（格子非法或被占？）"); return false; }
		return true;
	}
	if (op == "assert_can_plant") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("assert_can_plant: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kPlantNames.find(cmd.value("type", ""));
		if (it == kPlantNames.end()) { Fail("assert_can_plant: 未知植物类型"); return false; }
		const int row = cmd.value("row", 0);
		const int col = cmd.value("col", 0);
		const bool expected = cmd.value("expected", true);
		const bool actual = gs->GetBoard()->CanPlantAt(it->second, row, col);
		if (actual != expected) {
			Fail("assert_can_plant: 判定不符 row=" + std::to_string(row)
				+ " col=" + std::to_string(col));
			return false;
		}
		return true;
	}
	if (op == "spawn_bullet") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("spawn_bullet: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kBulletNames.find(cmd.value("type", ""));
		if (it == kBulletNames.end()) {
			Fail("spawn_bullet: type 必须是 BULLET_PEA/BULLET_SNOWPEA/BULLET_PUFF");
			return false;
		}
		Bullet* bullet = gs->GetBoard()->CreateBullet(it->second, cmd.value("row", 0),
			Vector(cmd.value("x", 100.0f), cmd.value("y", 300.0f)));
		if (!bullet) { Fail("spawn_bullet: CreateBullet 返回空"); return false; }
		bullet->SetVelocityX(cmd.value("velocityX", 290.0f));
		bullet->SetVelocityY(cmd.value("velocityY", 0.0f));
		bullet->SetBulletDamage(cmd.value("damage", 20));
		return true;
	}
	if (op == "spawn_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("spawn_zombie: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kZombieNames.find(cmd.value("type", ""));
		if (it == kZombieNames.end()) { Fail("未知僵尸类型: " + cmd.value("type", "")); return false; }
		Zombie* z = gs->GetBoard()->CreateZombie(it->second,
			cmd.value("row", 0), cmd.value("x", 900.0f));
		if (!z) { Fail("CreateZombie 返回空"); return false; }
		if (cmd.value("slowed", false)) {
			z->SetCooldown(cmd.value("slowDuration", 20.0f));
		}
		if (cmd.value("frozen", false) && !z->StartFrozen()) {
			Fail("spawn_zombie: frozen=true 但目标不能进入冻结");
			return false;
		}
		return true;
	}
	if (op == "spawn_wave_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("spawn_wave_zombie: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kZombieNames.find(cmd.value("type", ""));
		if (it == kZombieNames.end()) { Fail("spawn_wave_zombie: 未知僵尸类型"); return false; }
		const int roll = cmd.value("mutationRoll", 0);
		if (roll < 1 || roll > 100) { Fail("spawn_wave_zombie: mutationRoll 必须为 1..100"); return false; }
		Board* board = gs->GetBoard();
		const ZombieType actual = board->ResolveWaveZombieType(it->second, roll);
		// 与正式 TrySummonZombie 一致：超过每波上限的候选被跳过，不生成回退类型。
		if (actual == ZombieType::NUM_ZOMBIE_TYPES) return true;
		const int row = cmd.value("row", 0);
		if (!board->CanSpawnZombieInRow(actual, row)) {
			Fail("spawn_wave_zombie: 目标行与正式地形规则不兼容");
			return false;
		}
		const ZombieType terrainType = board->ResolveTerrainZombieType(actual, row);
		if (!board->CreateZombie(terrainType, row, cmd.value("x", 900.0f))) {
			Fail("spawn_wave_zombie: CreateZombie 返回空");
			return false;
		}
		return true;
	}
	if (op == "summon_next_wave") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("summon_next_wave: 不在 GameScene 或 Board 为空"); return false; }
		const int count = cmd.value("count", 1);
		if (count < 1 || count > 100) { Fail("summon_next_wave: count 必须在 1～100"); return false; }
		for (int i = 0; i < count; ++i) gs->GetBoard()->SummonNextWave();
		return true;
	}
	if (op == "damage_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("damage_zombie: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		const int row = cmd.value("row", -1);     // -1 = 不过滤行
		const int index = cmd.value("index", 0);  // 行过滤后按 ID 升序第 index 只
		const int damage = cmd.value("damage", 0);
		if (damage <= 0) { Fail("damage_zombie: damage 必须大于 0"); return false; }
		auto sourceIt = kDamageSourceNames.find(cmd.value("source", "OTHER"));
		if (sourceIt == kDamageSourceNames.end()) { Fail("damage_zombie: source 必须是 PLANT/ZOMBIE/OTHER"); return false; }
		int seen = 0;
		for (int id : board->mEntityManager.GetAllZombieIDs()) {
			Zombie* z = board->mEntityManager.GetZombie(id);
			if (!z) continue;
			if (row >= 0 && z->mRow != row) continue;
			if (seen++ == index) {
				// 走正式受伤链（来源词条/护盾/头盔/断肢断头/免伤），用于验证而非直接 Die。
				z->TakeDamage(damage, sourceIt->second, cmd.value("penetrateShield", false));
				return true;
			}
		}
		Fail("damage_zombie: 未找到目标僵尸 (row=" + std::to_string(row)
			+ ", index=" + std::to_string(index) + ")");
		return false;
	}
	if (op == "damage_plant") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("damage_plant: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		const int row = cmd.value("row", -1);     // -1 = 不过滤行
		const int col = cmd.value("col", -1);     // -1 = 不过滤列
		const int index = cmd.value("index", 0);  // 过滤后按 ID 升序第 index 株
		const int damage = cmd.value("damage", 0);
		if (damage <= 0) { Fail("damage_plant: damage 必须大于 0"); return false; }
		auto sourceIt = kDamageSourceNames.find(cmd.value("source", "OTHER"));
		if (sourceIt == kDamageSourceNames.end()) { Fail("damage_plant: source 必须是 PLANT/ZOMBIE/OTHER"); return false; }
		int seen = 0;
		for (int id : board->mEntityManager.GetAllPlantIDs()) {
			Plant* p = board->mEntityManager.GetPlant(id);
			if (!p) continue;
			if (row >= 0 && p->mRow != row) continue;
			if (col >= 0 && p->mColumn != col) continue;
			if (seen++ == index) {
				p->TakeDamage(damage, sourceIt->second);
				return true;
			}
		}
		Fail("damage_plant: 未找到目标植物 (row=" + std::to_string(row)
			+ ", col=" + std::to_string(col) + ", index=" + std::to_string(index) + ")");
		return false;
	}
	if (op == "add_perk") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("add_perk: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kPerkNames.find(cmd.value("type", ""));
		if (it == kPerkNames.end()) { Fail("未知词条类型: " + cmd.value("type", "")); return false; }
		int count = cmd.value("count", 1);
		for (int i = 0; i < count; ++i) gs->GetBoard()->GetPerkManager().AddPerk(it->second);
		return true;
	}
	if (op == "survival_perk_open") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("survival_perk_open: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->GetBoard()->mIsSurvival) { Fail("survival_perk_open: 非生存关"); return false; }
		gs->BeginSurvivalPerkSelect();
		return true;
	}
	if (op == "survival_perk_pick") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("survival_perk_pick: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->IsPerkSelectActive()) { Fail("survival_perk_pick: 当前无词条选择"); return false; }
		int index = cmd.value("index", -1);   // -1 = 放弃当前一次机会
		gs->ApplyPerkSelection(index);
		return true;
	}
	if (op == "survival_perk_refresh") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("survival_perk_refresh: 不在 GameScene 或 Board 为空"); return false; }
		if (!gs->IsPerkSelectActive()) { Fail("survival_perk_refresh: 当前无词条选择"); return false; }
		if (!gs->RefreshSurvivalPerkSelection()) { Fail("survival_perk_refresh: 本轮刷新次数已用完"); return false; }
		return true;
	}
	if (op == "show_zombie_hp") {
		GameAPP::GetInstance().mShowZombieHP = cmd.value("on", true);   // 调试：游戏内绘制僵尸血量
		return true;
	}
	if (op == "charm_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("charm_zombie: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		const int row = cmd.value("row", -1);     // -1 = 不过滤行
		const int index = cmd.value("index", 0);  // 行过滤后按 ID 升序第 index 只
		int seen = 0;
		for (int id : board->mEntityManager.GetAllZombieIDs()) {
			Zombie* z = board->mEntityManager.GetZombie(id);
			if (!z) continue;
			if (row >= 0 && z->mRow != row) continue;
			if (seen++ == index) {
				z->StartMindControlled();   // 不可魅惑目标是 no-op：脚本用 dump_state 的 mindControlled 断言
				return true;
			}
		}
		Fail("charm_zombie: 未找到目标僵尸 (row=" + std::to_string(row) + ", index=" + std::to_string(index) + ")");
		return false;
	}
	if (op == "screenshot") {
		const std::string name = cmd.value("name", "shot.png");
		auto* renderer = GameAPP::GetInstance().GetVulkanRenderer();
		if (!renderer) { Fail("screenshot: renderer 为空"); return false; }
		renderer->RequestCapture(mOutDir + "/" + name);
		// 本帧到此为止：截图在本帧 EndFrame 落盘，避免同帧第二个 screenshot 覆盖请求
		mBreakFrame = true;
		return true;
	}
	if (op == "dump_state") {
		nlohmann::json out;
		if (!BuildStateJson("dump_state", out)) return false;
		const std::string name = cmd.value("name", "state.json");
		std::ofstream of(mOutDir + "/" + name, std::ios::trunc);
		if (!of) { Fail("dump_state: 无法写 " + name); return false; }
		try {
			of << out.dump(2);
		}
		catch (const std::exception& e) {
			Fail("dump_state: JSON 序列化失败: " + std::string(e.what()));
			return false;
		}
		return true;
	}
	if (op == "assert_state") {
		// { "op":"assert_state", "path":"perks.stacks.PLANT_DAMAGE_UP", "equals":2 }
		// path 点分嵌套，纯数字段视为数组下标（如 "zombies.0.mindControlled"）；
		// equals 与实际值用 nlohmann::json operator== 严格比对，不匹配 Fail → exit 1。
		nlohmann::json state;
		if (!BuildStateJson("assert_state", state)) return false;
		const std::string path = cmd.value("path", "");
		if (path.empty()) { Fail("assert_state: 缺少 path"); return false; }
		const bool hasEquals = cmd.contains("equals");
		const bool hasAtLeast = cmd.contains("atLeast");
		const bool hasAtMost = cmd.contains("atMost");
		if (!hasEquals && !hasAtLeast && !hasAtMost) {
			Fail("assert_state: 缺少 equals/atLeast/atMost (path=" + path + ")");
			return false;
		}

		const nlohmann::json* cur = &state;
		size_t begin = 0;
		while (begin <= path.size()) {
			const size_t dot = path.find('.', begin);
			const std::string seg = path.substr(begin, dot == std::string::npos ? std::string::npos : dot - begin);
			if (cur->is_array() && !seg.empty()
				&& seg.find_first_not_of("0123456789") == std::string::npos) {
				const size_t idx = static_cast<size_t>(std::stoul(seg));
				if (idx >= cur->size()) {
					Fail("assert_state: 下标越界 \"" + seg + "\" (path=" + path
						+ ", size=" + std::to_string(cur->size()) + ")");
					return false;
				}
				cur = &(*cur)[idx];
			}
			else if (cur->is_object() && cur->contains(seg)) {
				cur = &(*cur)[seg];
			}
			else {
				Fail("assert_state: path 段不存在 \"" + seg + "\" (path=" + path + ")");
				return false;
			}
			if (dot == std::string::npos) break;
			begin = dot + 1;
		}

		if (hasEquals) {
			const nlohmann::json& expected = cmd["equals"];
			if (*cur != expected) {
				Fail("assert_state: 断言失败 path=" + path
					+ " 期望=" + expected.dump() + " 实际=" + cur->dump());
				return false;
			}
			Log("assert_state OK: " + path + " == " + expected.dump());
		}
		if (hasAtLeast || hasAtMost) {
			if (!cur->is_number()
				|| (hasAtLeast && !cmd["atLeast"].is_number())
				|| (hasAtMost && !cmd["atMost"].is_number())) {
				Fail("assert_state: atLeast/atMost 只支持数值 (path=" + path + ")");
				return false;
			}
			const double actual = cur->get<double>();
			if (hasAtLeast && actual < cmd["atLeast"].get<double>()) {
				Fail("assert_state: 断言失败 path=" + path + " 实际=" + cur->dump()
					+ " 小于下限=" + cmd["atLeast"].dump());
				return false;
			}
			if (hasAtMost && actual > cmd["atMost"].get<double>()) {
				Fail("assert_state: 断言失败 path=" + path + " 实际=" + cur->dump()
					+ " 大于上限=" + cmd["atMost"].dump());
				return false;
			}
			Log("assert_state OK: " + path + " in numeric bounds");
		}
		return true;
	}
	if (op == "quit") {
		Log("done cmd#" + std::to_string(mIndex) + " (quit)");
		Finish();
		return false;   // Finish 已停机，不再推进
	}

	if (op == "key") {
		const std::string name = cmd.value("name", "");
		auto it = kKeyNames.find(name);
		if (it == kKeyNames.end()) { Fail("未知按键名: " + name); return false; }
		const SDL_Keycode key = it->second;
		const std::string action = cmd.value("action", "press");

		auto pushKey = [&](Uint32 type) {
			SDL_Event ev{};
			ev.type = type;
			ev.key.keysym.sym = key;
			SDL_PushEvent(&ev);
		};

		if (action == "down") { pushKey(SDL_KEYDOWN); return true; }
		if (action == "up")   { pushKey(SDL_KEYUP);   return true; }
		if (action == "press") {
			// 跨帧：down 帧推 KEYDOWN（下一帧 poll 置 PRESSED，场景读到按下沿），
			// 下一帧推 KEYUP（再下一帧 poll 置 RELEASED）。mInputPhase=0 即"下帧收尾"。
			if (mInputPhase < 0) { pushKey(SDL_KEYDOWN); mInputPhase = 0; return false; }
			pushKey(SDL_KEYUP);
			return true;
		}
		Fail("未知 key action: " + action);
		return false;
	}

	if (op == "click") {
		float x = 0.0f, y = 0.0f;
		// "target":"trophy"：执行时从 Board 实时解析奖杯坐标（僵尸死亡位置受帧时序影响，
		// 静态写死坐标会漂移脱靶）；仍走下方 SDL_PushEvent 合成输入路径。
		const std::string target = cmd.value("target", "");
		if (target == "trophy") {
			GameScene* gs = CurrentGameScene();
			if (!gs || !gs->GetBoard()) { Fail("click target=trophy: 不在 GameScene 或 Board 为空"); return false; }
			auto trophy = gs->GetBoard()->mTrophy.lock();
			if (!trophy) { Fail("click target=trophy: 场上没有奖杯"); return false; }
			const Vector pos = trophy->GetPosition();
			x = pos.x;
			y = pos.y;
		}
		else if (!target.empty()) { Fail("未知 click target: " + target); return false; }
		else {
			// 显式查在：缺字段时不静默回落到 (0,0) 点击，分别精确报错
			if (!cmd.contains("x")) { Fail("click 缺 x 字段"); return false; }
			if (!cmd.contains("y")) { Fail("click 缺 y 字段"); return false; }
			x = cmd.value("x", 0.0f);
			y = cmd.value("y", 0.0f);
		}
		const std::string btnName = cmd.value("button", "left");
		auto bit = kMouseButtonNames.find(btnName);
		if (bit == kMouseButtonNames.end()) { Fail("未知鼠标按钮: " + btnName); return false; }
		const Uint8 button = bit->second;
		const int holdFrames = std::max(1, cmd.value("hold_frames", 1));

		// 跨帧状态机：mInputPhase<0 推 移动+按下并置剩余保持帧；>0 递减保持；==0 推松开完成。
		if (mInputPhase < 0) {
			SDL_Event mv{};
			mv.type = SDL_MOUSEMOTION;
			mv.motion.x = static_cast<Sint32>(x);   // AutoTest 窗口 scale=1，逻辑坐标即屏幕像素
			mv.motion.y = static_cast<Sint32>(y);
			SDL_PushEvent(&mv);

			SDL_Event down{};
			down.type = SDL_MOUSEBUTTONDOWN;
			down.button.button = button;
			down.button.x = static_cast<Sint32>(x);
			down.button.y = static_cast<Sint32>(y);
			SDL_PushEvent(&down);

			mInputPhase = holdFrames;
			return false;
		}
		if (mInputPhase > 0) { --mInputPhase; return false; }

		SDL_Event up{};
		up.type = SDL_MOUSEBUTTONUP;
		up.button.button = button;
		up.button.x = static_cast<Sint32>(x);
		up.button.y = static_cast<Sint32>(y);
		SDL_PushEvent(&up);
		return true;
	}

	Fail("未知命令 op=\"" + op + "\"");
	return false;
}

bool TestDriver::BuildStateJson(const std::string& opName, nlohmann::json& out)
{
	GameScene* gs = CurrentGameScene();
	if (!gs || !gs->GetBoard()) { Fail(opName + ": 不在 GameScene 或 Board 为空"); return false; }
	Board* board = gs->GetBoard();
	auto& gameApp = GameAPP::GetInstance();

	out["boardState"] = BoardStateName(board->mBoardState);
	out["chooseCardReady"] = gs->IsChooseCardReady();
	out["level"] = board->mLevel;
	out["levelName"] = board->mLevelName;
	out["background"] = BackgroundName(board->mBackGround);
	out["rows"] = board->mRows;
	out["columns"] = board->mColumns;
	out["supportsWeather"] = board->SupportsWeather();
	out["poolRows"] = nlohmann::json::array();
	for (int row = 0; row < board->mRows; ++row) {
		if (board->IsPoolRow(row)) out["poolRows"].push_back(row);
	}
	out["sun"] = board->mSun;
	out["wave"] = board->mCurrentWave;
	out["maxWave"] = board->mMaxWave;
	out["zombieNumber"] = board->mZombieNumber;
	out["hostileZombieCountForMusic"] = board->GetHostileZombieCountForMusic();
	out["mowerCount"] = static_cast<int>(board->mEntityManager.GetAllMowerIDs().size());
	int movingMowerCount = 0;
	out["mowers"] = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllMowerIDs()) {
		Mower* mower = board->mEntityManager.GetMower(id);
		if (!mower) continue;
		const bool moving = mower->mState == MowerState::MOVING;
		if (moving) ++movingMowerCount;
		out["mowers"].push_back({
			{ "id", id }, { "row", mower->mRow },
			{ "type", MowerTypeName(mower->mMowerType) },
			{ "state", moving ? "MOVING" : "IDLE" },
			{ "height", MowerHeightName(mower->mMowerHeight) },
			{ "track", mower->GetCurrentTrackName() },
			{ "visualY", mower->GetVisualPosition().y },
			{ "xInt", static_cast<int>(std::lround(mower->GetPosition().x)) },
		});
	}
	out["movingMowerCount"] = movingMowerCount;
	out["devNoCooldown"] = GameAPP::mDevNoCooldown;
	out["devFreePlant"] = GameAPP::mDevFreePlant;
	out["devSpawnPaused"] = GameAPP::mDevSpawnPaused;
	out["adventureLevel"] = gameApp.mAdventureLevel;
	out["haveCardCount"] = static_cast<int>(gameApp.mHaveCards.size());
	out["haveCards"] = nlohmann::json::array();
	for (PlantType type : gameApp.mHaveCards)
		out["haveCards"].push_back(PlantTypeName(type));

	// 奖杯（在场时给坐标，否则 null）——胜利路径冒烟测试的断言抓手
	if (auto trophy = board->mTrophy.lock()) {
		out["trophy"] = { { "x", trophy->GetPosition().x }, { "y", trophy->GetPosition().y } };
	}
	else {
		out["trophy"] = nullptr;
	}

	// 弹坑（毁灭菇）——阻种/消退冒烟的断言抓手；timeLeftInt 整数投影供 equals，浮点勿断言
	out["craters"] = nlohmann::json::array();
	for (auto& weak : board->mCraters) {
		auto crater = weak.lock();
		if (!crater || !crater->IsActive()) continue;
		out["craters"].push_back({
			{ "row", crater->mRow }, { "col", crater->mColumn },
			{ "timeLeftInt", static_cast<int>(crater->mTimeLeft) },
		});
	}

	// 屏幕抖动：shakeOn 整数投影供 equals（1=抖动中）；offset 浮点仅供人工排查，勿断言
	{
		const Vector shake = board->GetShakeOffset();
		out["shake"] = {
			{ "shakeOn", (shake.x != 0.0f || shake.y != 0.0f) ? 1 : 0 },
			{ "offsetX", shake.x }, { "offsetY", shake.y },
		};
	}

	// 黑夜天气：倍率与 alpha 另给整数投影，避免 AutoTest 对浮点做严格 equals。
	{
		const float zombieRain = board->GetZombieRainSpeedMultiplier();
		const float hostileWind = board->GetZombieWindMoveMultiplier(false);
		const float charmedWind = board->GetZombieWindMoveMultiplier(true);
		const float gustDrift = board->GetZombieGustDriftVelocity();
		const float plantRain = board->GetPlantRainActionSpeedMultiplier();
		const float weatherPressure = board->GetWeatherPressureFactor();
		const float perkAttack = static_cast<float>(
			board->GetPerkManager().GetPlantAttackSpeedMultiplier());
		out["weather"] = {
			{ "intensity", RainIntensityName(board->GetRainIntensity()) },
			{ "rainEffect", board->GetRainVisualEffectName() },
			{ "previousIntensity", RainIntensityName(board->GetPreviousRainIntensity()) },
			{ "initialized", board->IsWeatherInitialized() },
			{ "transitionOn", board->IsWeatherTransitionActive() },
			{ "transitionRemaining", board->GetWeatherTransitionTimer() },
			{ "canIntensify", board->CanRainIntensify() },
			{ "canHold", board->CanRainHold() },
			{ "forecastReady", board->HasWeatherForecast() },
			{ "forecastIntensity", RainIntensityName(board->GetForecastRainIntensity()) },
			{ "lockedActualIntensity", RainIntensityName(board->GetActualForecastRainIntensity()) },
			{ "forecastPlausible", board->IsWeatherForecastPlausible() },
			{ "forecastAccuracyPct", board->GetCurrentWeatherForecastAccuracyPercent() },
			{ "weakWeatherPhasesSinceHeavy", board->GetWeakWeatherPhasesSinceHeavy() },
			{ "heavyWeatherForced", board->IsHeavyWeatherForced() },
			{ "newClearWeight", board->GetCurrentNewWeatherWeight(RainIntensity::CLEAR) },
			{ "newLightWeight", board->GetCurrentNewWeatherWeight(RainIntensity::LIGHT) },
			{ "newMediumWeight", board->GetCurrentNewWeatherWeight(RainIntensity::MEDIUM) },
			{ "newHeavyWeight", board->GetCurrentNewWeatherWeight(RainIntensity::HEAVY) },
			{ "remaining", board->GetWeatherTimer() },
			{ "lightningRemaining", board->GetLightningTimer() },
			{ "pressurePct", static_cast<int>(std::lround(weatherPressure * 100.0f)) },
			{ "zombieSpeedPct", static_cast<int>(std::lround(zombieRain * 100.0f)) },
			{ "typhoonStrength", TyphoonStrengthName(board->GetTyphoonStrength()) },
			{ "typhoonChancePct", board->GetCurrentTyphoonChancePercent() },
			{ "heavyPhasesWithoutTyphoon", board->GetHeavyPhasesWithoutTyphoon() },
			{ "eliteDancersSpawnedThisWave", board->GetEliteDancersSpawnedThisWave() },
			{ "reinforcedDoorsSpawnedThisWave", board->GetReinforcedDoorsSpawnedThisWave() },
			{ "typhoonDecayRemaining", board->GetTyphoonStrengthTimer() },
			{ "windDirection", WindDirectionName(board->GetWindDirection()) },
			{ "windDirectionRemaining", board->GetWindDirectionTimer() },
			{ "windGustRemaining", board->GetWindGustTimer() },
			{ "gustsRemaining", board->GetTyphoonGustsRemaining() },
			{ "gustActive", board->IsTyphoonGustActive() },
			{ "activeGustStrength", TyphoonStrengthName(board->GetActiveGustStrength()) },
			{ "activeGustDirection", WindDirectionName(board->GetActiveGustDirection()) },
			{ "activeGustRemainingMs", static_cast<int>(std::lround(
				board->GetActiveGustTimer() * 1000.0f)) },
			{ "activeGustPlantMoveRemainingMs", static_cast<int>(std::lround(
				board->GetActiveGustPlantMoveTimer() * 1000.0f)) },
			{ "activeGustPlantMoved", board->HasActiveGustMovedPlants() },
			{ "zombieGustDriftSpeed", static_cast<int>(std::lround(gustDrift)) },
			{ "gustWarning", board->IsTyphoonGustWarning() },
			{ "lastGustMovedPlants", board->GetLastTyphoonMovedPlants() },
			{ "lastGustLostPlants", board->GetLastTyphoonLostPlants() },
			{ "hostileWindMovePct", static_cast<int>(std::lround(hostileWind * 100.0f)) },
			{ "charmedWindMovePct", static_cast<int>(std::lround(charmedWind * 100.0f)) },
			{ "hostileCombinedMovePct", static_cast<int>(std::lround(zombieRain * hostileWind * 100.0f)) },
			{ "charmedCombinedMovePct", static_cast<int>(std::lround(zombieRain * charmedWind * 100.0f)) },
			{ "plantActionSpeedPct", static_cast<int>(std::lround(plantRain * 100.0f)) },
			{ "overlayAlpha", static_cast<int>(std::lround(board->GetRainOverlayAlpha())) },
			{ "rainSoundPlaying", AudioSystem::IsLoopingSoundPlaying(ResourceKeys::Sounds::SOUND_RAIN) },
			{ "combinedAttackIntervalOn1500",
				static_cast<int>(1500.0f / (plantRain * perkAttack) + 0.5f) },
			{ "screenFlashOn", gs->IsScreenFlashActive() },
			{ "screenFlashPeakAlpha", static_cast<int>(std::lround(gs->GetScreenFlashPeakAlpha())) },
			{ "panelSlidePct", static_cast<int>(std::lround(gs->GetWeatherPanelSlide() * 100.0f)) },
			{ "currentNoticeOn", gs->IsCurrentWeatherNoticeActive() },
			{ "currentNoticeRemainingMs", static_cast<int>(std::lround(
				gs->GetCurrentWeatherNoticeTimer() * 1000.0f)) },
			{ "forecastFailureOn", gs->IsWeatherForecastFailureActive() },
			{ "forecastFailureRemainingMs", static_cast<int>(std::lround(
				gs->GetWeatherForecastFailureTimer() * 1000.0f)) },
			{ "failedForecastIntensity", RainIntensityName(gs->GetFailedForecastRainIntensity()) },
			{ "actualForecastIntensity", RainIntensityName(gs->GetActualForecastRainIntensity()) },
		};
	}

	out["survivalRound"] = board->mIsSurvival ? board->mSurvivalRound : -1;
	// 出怪池不分模式都 dump：冒险关卡验证 spawnlists.json 也要抓手（原先只在生存模式导出）
	out["spawnList"] = nlohmann::json::array();
	for (ZombieType t : board->GetSpawnZombieList())
		out["spawnList"].push_back(ZombieTypeName(t));
	out["spawnTypeCount"] = static_cast<int>(board->GetSpawnZombieList().size());

	int charredZombieCount = 0;
	for (const auto& object : GameObjectManager::GetInstance().GetAllGameObjects()) {
		if (object && object->IsActive() && dynamic_cast<ZombieCharred*>(object.get())) {
			++charredZombieCount;
		}
	}
	out["charredZombieCount"] = charredZombieCount;

	out["zombies"] = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllZombieIDs()) {
		Zombie* z = board->mEntityManager.GetZombie(id);
		if (!z) continue;
		const Vector pos = z->GetPosition();
		const auto anim = z->GetAnimatorInternal();
		nlohmann::json zombieState = {
			{ "id", id },
			{ "type", ZombieTypeName(z->mZombieType) },
			{ "row", z->mRow },
			{ "x", pos.x }, { "y", pos.y },
			{ "xInt", static_cast<int>(std::lround(pos.x)) },
			{ "yInt", static_cast<int>(std::lround(pos.y)) },
			{ "bodyHealth", z->mBodyHealth }, { "bodyMaxHealth", z->mBodyMaxHealth },
			{ "helmHealth", z->mHelmHealth }, { "shieldHealth", z->mShieldHealth },
			{ "mindControlled", z->IsMindControlled() },
			{ "isEating", z->IsEating() },
			{ "hasHead", z->HasHead() }, { "hasArm", z->HasArm() },
			{ "slowCooldown", z->GetCooldownTimer() },
			// slowed 供 assert_state（bool 可 equals）；slowCooldown 浮点仅供肉眼核对勿断言
			{ "slowed", z->GetCooldownTimer() > 0.0f },
			// frozen 供 assert_state（bool 可 equals）；frozenTimer 浮点仅供肉眼核对勿断言
			{ "frozen", z->IsFrozen() },
			{ "frozenTimer", z->GetFrozenTimer() },
			{ "track", z->GetCurrentTrackName() },
			{ "flipX", anim && anim->GetFlipX() },
			{ "animPlaying", anim && anim->IsPlaying() },
			{ "animExtraSpeedPct", anim
				? static_cast<int>(std::lround(anim->GetExtraSpeedMultiplier() * 100.0f)) : 0 },
			{ "effectiveAnimSpeed", anim ? anim->EffectiveSpeed() : 0.0f },
			{ "freeHitsRemaining", z->mFreeHitsRemaining },
			// 铁门僵尸常规手臂（藏门后/啃食露出）当前可见性——手臂显隐类 bug 的断言抓手；
			// 无此轨道的僵尸 GetTrackVisible 安全返回 false。
			{ "armVisible", anim && anim->GetTrackVisible("Zombie_outerarm_hand") },
			// 铁门僵尸「持门手臂」（Zombie_*_screendoor*）可见性：死亡/断臂时本应随门消失。
			{ "doorArmVisible", anim && (anim->GetTrackVisible("Zombie_outerarm_screendoor")
				|| anim->GetTrackVisible("Zombie_innerarm_screendoor")
				|| anim->GetTrackVisible("Zombie_innerarm_screendoor_hand")) },
		};
		if (auto* elite = dynamic_cast<EliteDancerZombie*>(z)) {
			zombieState["eliteBackupCount"] = elite->GetActiveBackupCount();
			zombieState["eliteSummonRemainingMs"] = static_cast<int>(std::lround(
				elite->GetSummonTimer() * 1000.0f));
			zombieState["eliteTyphoonSpeedPct"] = static_cast<int>(std::lround(
				elite->GetTyphoonAbilitySpeedMultiplier() * 100.0f));
		}
		if (auto* pool = dynamic_cast<PoolNormalZombie*>(z)) {
			zombieState["inPool"] = pool->IsInPool();
			zombieState["poolLegsVisible"] = anim && (
				anim->GetTrackVisible("Zombie_innerleg_upper")
				|| anim->GetTrackVisible("Zombie_innerleg_lower")
				|| anim->GetTrackVisible("Zombie_innerleg_foot")
				|| anim->GetTrackVisible("Zombie_outerleg_upper")
				|| anim->GetTrackVisible("Zombie_outerleg_lower")
				|| anim->GetTrackVisible("Zombie_outerleg_foot"));
		}
		out["zombies"].push_back(std::move(zombieState));
	}
	out["zombieCount"] = static_cast<int>(out["zombies"].size());

	out["plants"] = nlohmann::json::array();
	int repeatingShootingHeadCount = 0;
	for (int id : board->mEntityManager.GetAllPlantIDs()) {
		Plant* p = board->mEntityManager.GetPlant(id);
		if (!p) continue;
		nlohmann::json plantState = {
			{ "id", id },
			{ "type", PlantTypeName(p->mPlantType) },
			{ "row", p->mRow }, { "col", p->mColumn },
			{ "health", p->mPlantHealth }, { "maxHealth", p->mPlantMaxHealth },
			{ "eaterCount", p->mEaterCount },
			{ "track", p->GetCurrentTrackName() },
			{ "logicalY", p->GetPosition().y },
			{ "visualY", p->GetVisualPosition().y },
		};
		if (auto* shooter = dynamic_cast<Shooter*>(p)) {
			if (const Animator* head = shooter->GetHeadAnimator()) {
				plantState["headTrack"] = head->GetCurrentTrackName();
				plantState["headAnimPlaying"] = head->IsPlaying();
				plantState["headAnimPlayState"] = PlayStateName(head->GetPlayingState());
				if (head->IsPlaying() && head->GetPlayingState() == PlayState::PLAY_REPEAT
					&& head->GetCurrentTrackName() == "anim_shooting") {
					++repeatingShootingHeadCount;
				}
			}
		}
		if (auto* lilyPad = dynamic_cast<LilyPad*>(p)) {
			plantState["biteProtected"] = lilyPad->IsBiteProtected();
		}
		out["plants"].push_back(std::move(plantState));
	}
	out["plantCount"] = static_cast<int>(out["plants"].size());
	out["cells"] = nlohmann::json::array();
	for (int row = 0; row < board->mRows; ++row) {
		nlohmann::json rowState = nlohmann::json::array();
		for (int col = 0; col < board->mColumns; ++col) {
			Cell* cell = board->GetCell(row, col);
			Plant* under = cell ? board->mEntityManager.GetPlant(cell->GetUnderPlantID()) : nullptr;
			Plant* normal = cell ? board->mEntityManager.GetPlant(cell->GetNormalPlantID()) : nullptr;
			rowState.push_back({
				{ "under", under ? PlantTypeName(under->mPlantType) : "NONE" },
				{ "normal", normal ? PlantTypeName(normal->mPlantType) : "NONE" },
				{ "top", normal ? PlantTypeName(normal->mPlantType)
					: (under ? PlantTypeName(under->mPlantType) : "NONE") },
			});
		}
		out["cells"].push_back(std::move(rowState));
	}
	out["bullets"] = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllBulletIDs()) {
		Bullet* bullet = board->mEntityManager.GetBullet(id);
		if (!bullet) continue;
		const Vector pos = bullet->GetPosition();
		out["bullets"].push_back({
			{ "id", id },
			{ "type", static_cast<int>(bullet->mBulletType) },
			{ "row", bullet->mRow },
			{ "x", pos.x }, { "y", pos.y },
			{ "windAffected", bullet->IsTyphoonWindAffected() },
			{ "baseVelocityX", static_cast<int>(std::lround(bullet->GetVelocityX())) },
			{ "windVelocityX", static_cast<int>(std::lround(bullet->GetWindAdjustedVelocityX())) },
			{ "baseDamage", bullet->GetBulletDamage() },
			{ "windDamage", bullet->GetWindAdjustedDamage() },
		});
	}
	out["bulletCount"] = static_cast<int>(out["bullets"].size());
	out["repeatingShootingHeadCount"] = repeatingShootingHeadCount;

	{
		SurvivalPerkManager& pm = board->GetPerkManager();
		nlohmann::json stacks;
		stacks["PLANT_DAMAGE_UP"]      = pm.GetStacks(PerkType::PLANT_DAMAGE_UP);
		stacks["ZOMBIE_HEALTH_UP"]     = pm.GetStacks(PerkType::ZOMBIE_HEALTH_UP);
		stacks["ZOMBIE_DAMAGE_RESIST"] = pm.GetStacks(PerkType::ZOMBIE_DAMAGE_RESIST);
		stacks["ZOMBIE_DAMAGE_UP"]     = pm.GetStacks(PerkType::ZOMBIE_DAMAGE_UP);
		stacks["ZOMBIE_INVULN_HITS"]   = pm.GetStacks(PerkType::ZOMBIE_INVULN_HITS);
		stacks["PLANT_REGEN"]          = pm.GetStacks(PerkType::PLANT_REGEN);
		stacks["PLANT_ATTACK_SPEED"]   = pm.GetStacks(PerkType::PLANT_ATTACK_SPEED);
		stacks["PLANT_DAMAGE_REDUCTION"] = pm.GetStacks(PerkType::PLANT_DAMAGE_REDUCTION);
		stacks["PLANT_SUN_BONUS"]        = pm.GetStacks(PerkType::PLANT_SUN_BONUS);
		stacks["PLANT_CARD_RECHARGE"]    = pm.GetStacks(PerkType::PLANT_CARD_RECHARGE);
		nlohmann::json perks;
		perks["stacks"]              = stacks;
		perks["zombieHealthMult"]    = pm.GetZombieHealthMultiplier();
		perks["zombieHealthOn100"]   = static_cast<int>(100.0 * pm.GetZombieHealthMultiplier() + 0.5);
		perks["plantDamageOn100"]    = pm.ScalePlantDamage(100);
		perks["damageToZombieOn100"] = pm.ScaleDamageToZombie(100);
		perks["damageToPlantOn100"]  = pm.ScaleDamageToPlant(100);
		perks["sunIncomeOn100"]      = pm.ScaleSunIncome(100);
		perks["zombieDamageMult"]    = pm.GetZombieDamageMultiplier();
		perks["zombieDamageOn100"]   = pm.ScaleZombieDamage(100);
		perks["zombieInvulnHits"]    = pm.GetZombieInvulnHits();
		perks["plantRegenPerPulse"]  = pm.GetPlantRegenPerPulse();
		perks["plantRegenCapOn300"]  = pm.GetPlantRegenHpCap(300);
		double asMult = pm.GetPlantAttackSpeedMultiplier();
		perks["plantAttackSpeedMult"] = asMult;                                  // 原始倍率
		perks["shootIntervalOn1500"]  = static_cast<int>(1500.0 / asMult + 0.5); // 整数化：1.5s 间隔被缩到多少 ms
		double rechargeMult = pm.GetPlantCardRechargeMultiplier();
		perks["plantCardRechargeMult"] = rechargeMult;
		perks["cardRechargeOn1000"] = static_cast<int>(1000.0 / rechargeMult + 0.5);
		out["perks"] = perks;
	}

	{
		nlohmann::json psel;
		psel["active"] = gs->IsPerkSelectActive();
		psel["offerCount"] = static_cast<int>(gs->GetCurrentPerkOffer().size());
		psel["currentPick"] = gs->GetPerkCurrentPick();
		psel["completedSteps"] = gs->GetPerkStepsCompleted();
		psel["completedPicks"] = gs->GetPerkPicksCompleted();
		psel["maxPicks"] = GameScene::SURVIVAL_PERK_PICKS_PER_ROUND;
		psel["refreshesRemaining"] = gs->GetPerkRefreshesRemaining();
		psel["maxRefreshes"] = GameScene::SURVIVAL_PERK_REFRESHES_PER_ROUND;
		nlohmann::json offers = nlohmann::json::array();
		for (const PerkPairing& pr : gs->GetCurrentPerkOffer()) {
			offers.push_back({
				{ "plant",  SurvivalPerkManager::GetInfo(pr.plant).key },
				{ "zombie", SurvivalPerkManager::GetInfo(pr.zombie).key },
			});
		}
		psel["offers"] = offers;
		out["perkSelect"] = psel;
	}

	return true;
}
