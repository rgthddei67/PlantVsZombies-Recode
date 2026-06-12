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
#include <filesystem>
#include <algorithm>
#include <unordered_map>

namespace {
#define PT(n) { #n, PlantType::n }
	const std::unordered_map<std::string, PlantType> kPlantNames = {
		PT(PLANT_PEASHOOTER), PT(PLANT_SUNFLOWER), PT(PLANT_CHERRYBOMB), PT(PLANT_WALLNUT),
		PT(PLANT_POTATOMINE), PT(PLANT_SNOWPEA), PT(PLANT_CHOMPER), PT(PLANT_REPEATER),
		PT(PLANT_PUFFSHROOM), PT(PLANT_SUNSHROOM), PT(PLANT_FUMESHROOM), PT(PLANT_GRAVEBUSTER),
		PT(PLANT_HYPNOSHROOM), PT(PLANT_SCAREDYSHROOM), PT(PLANT_ICESHROOM), PT(PLANT_DOOMSHROOM),
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
	const std::unordered_map<std::string, BoardState> kBoardStateNames = {
		{ "CHOOSE_CARD", BoardState::CHOOSE_CARD }, { "GAME", BoardState::GAME },
		{ "LOSE_GAME", BoardState::LOSE_GAME }, { "WIN", BoardState::WIN },
		{ "NONE", BoardState::NONE },
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
		GameScene* gs = CurrentGameScene();
		if (!gs || !gs->GetBoard()) { Fail("dump_state: 不在 GameScene 或 Board 为空"); return false; }
		Board* board = gs->GetBoard();
		nlohmann::json out;
		out["boardState"] = BoardStateName(board->mBoardState);
		out["sun"] = board->mSun;
		out["wave"] = board->mCurrentWave;
		out["zombieNumber"] = board->mZombieNumber;

		out["zombies"] = nlohmann::json::array();
		for (int id : board->mEntityManager.GetAllZombieIDs()) {
			Zombie* z = board->mEntityManager.GetZombie(id);
			if (!z) continue;
			const Vector pos = z->GetPosition();
			out["zombies"].push_back({
				{ "id", id },
				{ "type", ZombieTypeName(z->mZombieType) },
				{ "row", z->mRow },
				{ "x", pos.x }, { "y", pos.y },
				{ "bodyHealth", z->mBodyHealth }, { "bodyMaxHealth", z->mBodyMaxHealth },
				{ "helmHealth", z->mHelmHealth }, { "shieldHealth", z->mShieldHealth },
				{ "hasHead", z->HasHead() }, { "hasArm", z->HasArm() },
				{ "slowCooldown", z->GetCooldownTimer() },
				{ "track", z->GetCurrentTrackName() },
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
	if (op == "quit") {
		Log("done cmd#" + std::to_string(mIndex) + " (quit)");
		Finish();
		return false;   // Finish 已停机，不再推进
	}

	Fail("未知命令 op=\"" + op + "\"");
	return false;
}
