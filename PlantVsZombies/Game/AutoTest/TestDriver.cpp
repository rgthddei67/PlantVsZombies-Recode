#include "TestDriver.h"
#include "../../GameAPP.h"
#include "../../Renderer/VulkanRenderer.h"
#include "../../DeltaTime.h"
#include "../../Logger.h"
#include "../SceneManager.h"
#include "../GameScene.h"
#include "../ChooseCardUI.h"
#include "../Board.h"
#include "../Card.h"
#include "../CardComponent.h"
#include "../Plant/PlantType.h"
#include "../Plant/Plant.h"
#include "../Zombie/ZombieType.h"
#include "../Zombie/Zombie.h"
#include "../Trophy.h"   // dump_state 输出奖杯坐标
#include "../../Reanimation/Animator.h"   // dump_state 查询轨道可见性（如铁门僵尸手臂）
#include <filesystem>
#include <algorithm>
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
#define ZT(n) { #n, ZombieType::n }
	const std::unordered_map<std::string, ZombieType> kZombieNames = {
		ZT(ZOMBIE_NORMAL), ZT(ZOMBIE_TRAFFIC_CONE), ZT(ZOMBIE_POLEVAULTER), ZT(ZOMBIE_BUCKET),
		ZT(ZOMBIE_FASTBUCKET), ZT(ZOMBIE_NEWSPAPER), ZT(ZOMBIE_FASTPAPER), ZT(ZOMBIE_DOOR),
		ZT(ZOMBIE_FOOTBALL), ZT(ZOMBIE_DANCER), ZT(ZOMBIE_BACKUP_DANCER), ZT(ZOMBIE_DUCKY_TUBE),
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
		PK(PLANT_ATTACK_SPEED),
	};
#undef PK
	const std::unordered_map<std::string, BoardState> kBoardStateNames = {
		{ "CHOOSE_CARD", BoardState::CHOOSE_CARD }, { "GAME", BoardState::GAME },
		{ "LOSE_GAME", BoardState::LOSE_GAME }, { "WIN", BoardState::WIN },
		{ "NONE", BoardState::NONE },
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
	if (op == "force_survival_round") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("force_survival_round: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		if (!board->mIsSurvival) { Fail("force_survival_round: 非生存模式关卡"); return false; }
		int round = cmd.value("round", 1);
		if (round < 1) round = 1;
		board->mSurvivalRound = round;
		board->BuildSurvivalSpawnList(round);   // 公有方法，直接按轮重建出怪池
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
	if (op == "spawn_zombie") {
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("spawn_zombie: 不在 GameScene 或 Board 为空"); return false; }
		auto it = kZombieNames.find(cmd.value("type", ""));
		if (it == kZombieNames.end()) { Fail("未知僵尸类型: " + cmd.value("type", "")); return false; }
		Zombie* z = gs->GetBoard()->CreateZombie(it->second,
			cmd.value("row", 0), cmd.value("x", 900.0f));
		if (!z) { Fail("CreateZombie 返回空"); return false; }
		return true;
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
		int index = cmd.value("index", -1);   // -1 = 跳过
		gs->ApplyPerkSelection(index);
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
		if (!cmd.contains("equals")) { Fail("assert_state: 缺少 equals (path=" + path + ")"); return false; }

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

		const nlohmann::json& expected = cmd["equals"];
		if (*cur != expected) {
			Fail("assert_state: 断言失败 path=" + path
				+ " 期望=" + expected.dump() + " 实际=" + cur->dump());
			return false;
		}
		Log("assert_state OK: " + path + " == " + expected.dump());
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

	out["boardState"] = BoardStateName(board->mBoardState);
	out["sun"] = board->mSun;
	out["wave"] = board->mCurrentWave;
	out["zombieNumber"] = board->mZombieNumber;
	out["devNoCooldown"] = GameAPP::mDevNoCooldown;
	out["devFreePlant"] = GameAPP::mDevFreePlant;
	out["devSpawnPaused"] = GameAPP::mDevSpawnPaused;

	// 奖杯（在场时给坐标，否则 null）——胜利路径冒烟测试的断言抓手
	if (auto trophy = board->mTrophy.lock()) {
		out["trophy"] = { { "x", trophy->GetPosition().x }, { "y", trophy->GetPosition().y } };
	}
	else {
		out["trophy"] = nullptr;
	}

	out["survivalRound"] = board->mIsSurvival ? board->mSurvivalRound : -1;
	// 出怪池不分模式都 dump：冒险关卡验证 spawnlists.json 也要抓手（原先只在生存模式导出）
	out["spawnList"] = nlohmann::json::array();
	for (ZombieType t : board->GetSpawnZombieList())
		out["spawnList"].push_back(ZombieTypeName(t));

	out["zombies"] = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllZombieIDs()) {
		Zombie* z = board->mEntityManager.GetZombie(id);
		if (!z) continue;
		const Vector pos = z->GetPosition();
		const auto anim = z->GetAnimatorInternal();
		out["zombies"].push_back({
			{ "id", id },
			{ "type", ZombieTypeName(z->mZombieType) },
			{ "row", z->mRow },
			{ "x", pos.x }, { "y", pos.y },
			{ "bodyHealth", z->mBodyHealth }, { "bodyMaxHealth", z->mBodyMaxHealth },
			{ "helmHealth", z->mHelmHealth }, { "shieldHealth", z->mShieldHealth },
			{ "mindControlled", z->IsMindControlled() },
			{ "hasHead", z->HasHead() }, { "hasArm", z->HasArm() },
			{ "slowCooldown", z->GetCooldownTimer() },
			// slowed 供 assert_state（bool 可 equals）；slowCooldown 浮点仅供肉眼核对勿断言
			{ "slowed", z->GetCooldownTimer() > 0.0f },
			// frozen 供 assert_state（bool 可 equals）；frozenTimer 浮点仅供肉眼核对勿断言
			{ "frozen", z->IsFrozen() },
			{ "frozenTimer", z->GetFrozenTimer() },
			{ "track", z->GetCurrentTrackName() },
			{ "freeHitsRemaining", z->mFreeHitsRemaining },
			// 铁门僵尸常规手臂（藏门后/啃食露出）当前可见性——手臂显隐类 bug 的断言抓手；
			// 无此轨道的僵尸 GetTrackVisible 安全返回 false。
			{ "armVisible", anim && anim->GetTrackVisible("Zombie_outerarm_hand") },
			// 铁门僵尸「持门手臂」（Zombie_*_screendoor*）可见性：死亡/断臂时本应随门消失。
			{ "doorArmVisible", anim && (anim->GetTrackVisible("Zombie_outerarm_screendoor")
				|| anim->GetTrackVisible("Zombie_innerarm_screendoor")
				|| anim->GetTrackVisible("Zombie_innerarm_screendoor_hand")) },
		});
	}

	out["plants"] = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllPlantIDs()) {
		Plant* p = board->mEntityManager.GetPlant(id);
		if (!p) continue;
		out["plants"].push_back({
			{ "id", id },
			{ "type", PlantTypeName(p->mPlantType) },
			{ "row", p->mRow }, { "col", p->mColumn },
			{ "health", p->mPlantHealth }, { "maxHealth", p->mPlantMaxHealth },
			{ "track", p->GetCurrentTrackName() },
		});
	}

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
		nlohmann::json perks;
		perks["stacks"]              = stacks;
		perks["zombieHealthMult"]    = pm.GetZombieHealthMultiplier();
		perks["plantDamageOn100"]    = pm.ScalePlantDamage(100);
		perks["damageToZombieOn100"] = pm.ScaleDamageToZombie(100);
		perks["zombieDamageMult"]    = pm.GetZombieDamageMultiplier();
		perks["zombieDamageOn100"]   = pm.ScaleZombieDamage(100);
		perks["zombieInvulnHits"]    = pm.GetZombieInvulnHits();
		perks["plantRegenPerPulse"]  = pm.GetPlantRegenPerPulse();
		perks["plantRegenCapOn300"]  = pm.GetPlantRegenHpCap(300);
		double asMult = pm.GetPlantAttackSpeedMultiplier();
		perks["plantAttackSpeedMult"] = asMult;                                  // 原始倍率
		perks["shootIntervalOn1500"]  = static_cast<int>(1500.0 / asMult + 0.5); // 整数化：1.5s 间隔被缩到多少 ms
		out["perks"] = perks;
	}

	{
		nlohmann::json psel;
		psel["active"] = gs->IsPerkSelectActive();
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
