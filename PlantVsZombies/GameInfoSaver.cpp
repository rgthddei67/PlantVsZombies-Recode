#include "GameInfoSaver.h"
#if defined(__ANDROID__)
#include <SDL2/SDL.h>
#endif
#include "GameAPP.h"
#include "./Game/Board.h"
#include "./Game/GameScene.h"
#include "./Game/AudioSystem.h"
#include "./Game/Bullet/Bullet.h"
#include "./Game/Plant/Plant.h"
#include "./Game/Zombie/Zombie.h"
#include "./Game/LawnMower.h"
#include "./Game/GameProgress.h"
#include "./Game/Sun.h"
#include "./Game/Trophy.h"
#include "./Game/Crater.h"
#include "./Game/Bullet/BulletType.h"
#include "./Game/CardSlotManager.h"
#include "./Game/Card.h"
#include "./Game/CardComponent.h"
#include "./Game/TransformComponent.h"
#include "./Game/GameObjectManager.h"
#include "./Game/AnimatedObject.h"
#include "Logger.h"

namespace {
	// ---- 存档根目录 -------------------------------------------------------------
	// 桌面(Win/Linux)：沿用 "./saves"(相对 CWD，行为不变)。
	// Android：CWD 不可写，改用 SDL 提供的应用私有可写目录。
	const std::string& GetSaveRoot() {
#if defined(__ANDROID__)
		static const std::string root = []() -> std::string {
			char* p = SDL_GetPrefPath("PvZ", "PlantsVsZombies");
			if (!p) return "saves";                 // 极端兜底
			std::string s(p);
			SDL_free(p);
			if (!s.empty() && (s.back() == '/' || s.back() == '\\'))
				s.pop_back();                       // 去尾斜杠，拼接统一加 "/"
			return s;
		}();
		return root;
#else
		static const std::string root = "./saves";
		return root;
#endif
	}

	// ---- Animator 播放状态机的统一存读档 ----------------------------------------
	// 历史上只持久化 animTrack(当前轨道) + animFrame(当前帧)，读档时一律 PlayTrack(track)。
	// 但 PlayTrack 会把 mPlayingState 强制写成 PLAY_REPEAT，于是一只正在 PlayTrackOnce 的
	// 单位(长大/起跳/射击/喘气…)读档后会把那条一次性轨道当循环永远播放，再也切不回目标轨道。
	// 修复：把整台状态机(播放状态 + 目标轨道 + 回切速度 + 基础/clip 速度)都存下来，
	// 读档时按状态用 PlayTrackOnce 重建一次性播放。旧存档无新字段 → 默认 PLAY_REPEAT，
	// 行为与从前逐位一致(向后兼容)。Chomper/PaperZombie/PotatoMine 的 LoadExtraData 在本
	// 函数之后运行，仍可按需覆盖(它们还负责帧事件等本层无法序列化的东西)。
	void SaveAnimState(nlohmann::json& j, const AnimatedObject* obj) {
		j["animTrack"] = obj->GetCurrentTrackName();
		j["animFrame"] = obj->GetCurrentFrame();
		j["animSpeed"] = obj->GetAnimationSpeed();        // 基础速度(base)
		j["animClipSpeed"] = obj->GetClipSpeed();         // 当前轨道 clip 覆盖(0=回落 base)
		j["animPlayState"] = static_cast<int>(obj->GetPlayingState());
		j["animTargetTrack"] = obj->GetTargetTrack();     // PlayTrackOnce 播完后的回切轨道
		j["animTargetTrackSpeed"] = obj->GetTargetTrackSpeed();
	}

	void RestoreAnimState(const nlohmann::json& j, AnimatedObject* obj) {
		std::string track = j.value("animTrack", std::string{});
		if (track.empty()) return;

		float clipSpeed = j.value("animClipSpeed", 0.0f);   // 缺省 0 = 回落 base，与旧行为一致
		PlayState state = static_cast<PlayState>(
			j.value("animPlayState", static_cast<int>(PlayState::PLAY_REPEAT)));  // 旧存档→循环

		if (state == PlayState::PLAY_ONCE_TO || state == PlayState::PLAY_ONCE) {
			// 重建一次性播放：播完后切到目标轨道(PLAY_ONCE 时目标为空 = 播完即停)
			obj->PlayTrackOnce(track,
				j.value("animTargetTrack", std::string{}),
				clipSpeed, 0.0f,
				j.value("animTargetTrackSpeed", 0.0f));
		}
		else {
			obj->PlayTrack(track, clipSpeed);
		}

		// 基础速度与 clip 正交，单独恢复；仅当存档含该字段时才覆盖(旧存档不动，保持历史行为)
		if (j.contains("animSpeed")) {
			obj->SetAnimationSpeed(j.value("animSpeed", 1.0f));
		}
		// PlayTrack/PlayTrackOnce 会把帧重置到轨道起点，最后再恢复保存时的帧进度
		obj->SetCurrentFrame(j.value("animFrame", 0.0f));
	}
}

// 存档/读档的实际逻辑放在下列 *Impl 成员函数中（须为成员：Board 仅 friend 本类，自由
// 函数无私有成员访问权）；文件末尾的公有接口只做一层 try/catch 包裹（异常安全边界）。
// 命名加 Impl 后缀避免与同名公有方法构成无限递归调用。
bool GameInfoSaver::SavePlayerInfoImpl()
{
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：不碰玩家存档
	auto& gameApp = GameAPP::GetInstance();

	FileManager::CreateDirectory(GetSaveRoot());

	nlohmann::json j;
	j["vsync"] = gameApp.mVsync;
	j["fullscreen"] = gameApp.mFullscreen;
	j["difficulty"] = gameApp.Difficulty;
	j["adventureLevel"] = gameApp.mAdventureLevel;
	j["showPlantHP"] = gameApp.mShowPlantHP;
	j["showZombieHP"] = gameApp.mShowZombieHP;
	j["autoCollected"] = gameApp.mAutoCollected;
	j["soundVolume"] = AudioSystem::GetSoundVolume();
	j["musicVolume"] = AudioSystem::GetMusicVolume();
	j["havecards"] = gameApp.mHaveCards;

	return FileManager::SaveJsonFile(GetSaveRoot() + "/PlayerInfo.json", j);
}

bool GameInfoSaver::LoadPlayerInfoImpl()
{
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：全默认状态，保证确定性
	nlohmann::json j;
	if (!FileManager::LoadJsonFile(GetSaveRoot() + "/PlayerInfo.json", j))
		return false;

	auto& gameApp = GameAPP::GetInstance();
	gameApp.mVsync = j.value("vsync", false);
	gameApp.mFullscreen = j.value("fullscreen", false);
	gameApp.Difficulty = j.value("difficulty", 1);
	gameApp.mAdventureLevel = j.value("adventureLevel", 1);
	gameApp.mShowPlantHP = j.value("showPlantHP", false);
	gameApp.mShowZombieHP = j.value("showZombieHP", false);
	gameApp.mAutoCollected = j.value("autoCollected", true);
	gameApp.mHaveCards = j.value("havecards",
		std::vector<PlantType>{PlantType::PLANT_PEASHOOTER});

	AudioSystem::SetSoundVolume(j.value("soundVolume", 0.5f));
	AudioSystem::SetMusicVolume(j.value("musicVolume", 0.5f));

	return true;
}

bool GameInfoSaver::SaveLevelDataImpl(Board* board, CardSlotManager* manager)
{
	if (GameAPP::mAutoTestMode) return true;   // AutoTest：不写关卡存档
	const bool stateOk = (board->mBoardState == BoardState::GAME) ||
		(board->mIsSurvival && board->mBoardState == BoardState::CHOOSE_CARD);
	if (!stateOk) return false;
	FileManager::CreateDirectory(GetSaveRoot());

	nlohmann::json j;

	// Board 状态
	j["boardState"] = static_cast<int>(board->mBoardState);
	j["isSurvival"] = board->mIsSurvival;
	j["survivalRound"] = board->mSurvivalRound;
	if (board->mIsSurvival) {
		nlohmann::json perks;                    // 不直接写 j["perks"]：operator[] 会先物化成 null
		board->GetPerkManager().Save(perks);     // 零词条时 Save 不写任何键 → perks 仍为 null
		if (!perks.is_null()) j["perks"] = perks;   // 仅有词条时才落盘；否则省略，等同旧档天然兼容

		// 冻结本轮已 roll 出的随机出怪池：持久化实际 mSpawnZombieList，读档直接还原而非
		// 重 roll（见 Load 端）。否则退出重进/读档会重跑 BuildSurvivalSpawnList 刷新随机子集，
		// 玩家可借此反复刷出怪池直到阵容有利（原版生存出怪恒定，不可刷）。
		nlohmann::json spawnList = nlohmann::json::array();
		for (ZombieType t : board->GetSpawnZombieList())
			spawnList.push_back(static_cast<int>(t));
		j["spawnList"] = spawnList;
	}
	j["sun"] = board->mSun;
	j["currentWave"] = board->mCurrentWave;
	j["boardFrame"] = board->mBoardFrame;   // 舞王全队齐舞的节拍源，读档保节拍连续
	j["weatherInitialized"] = board->mWeatherInitialized;
	j["rainIntensity"] = static_cast<int>(board->mRainIntensity);
	j["previousRainIntensity"] = static_cast<int>(board->mPreviousRainIntensity);
	j["forecastRainIntensity"] = static_cast<int>(board->mForecastRainIntensity);
	j["actualForecastRainIntensity"] = static_cast<int>(board->mActualForecastRainIntensity);
	j["weatherTimer"] = board->mWeatherTimer;
	j["weatherTransitionTimer"] = board->mWeatherTransitionTimer;
	j["lightningTimer"] = board->mLightningTimer;
	j["rainCanIntensify"] = board->mRainCanIntensify;
	j["rainCanHold"] = board->mRainCanHold;
	j["weatherForecastReady"] = board->mWeatherForecastReady;
	j["typhoonStrength"] = static_cast<int>(board->mTyphoonStrength);
	j["windDirection"] = static_cast<int>(board->mWindDirection);
	j["typhoonStrengthTimer"] = board->mTyphoonStrengthTimer;
	j["windDirectionTimer"] = board->mWindDirectionTimer;
	j["windGustTimer"] = board->mWindGustTimer;
	j["typhoonGustsRemaining"] = board->mTyphoonGustsRemaining;
	j["typhoonGustActive"] = board->mTyphoonGustActive;
	j["activeGustStrength"] = static_cast<int>(board->mActiveGustStrength);
	j["activeGustDirection"] = static_cast<int>(board->mActiveGustDirection);
	j["activeGustDuration"] = board->mActiveGustDuration;
	j["activeGustTimer"] = board->mActiveGustTimer;
	j["activeGustPlantMoveTimer"] = board->mActiveGustPlantMoveTimer;
	j["activeGustPlantMoved"] = board->mActiveGustPlantMoved;
	j["heavyPhasesWithoutTyphoon"] = board->mHeavyPhasesWithoutTyphoon;
	j["currentWeatherNoticeTimer"] = board->mGameScene
		? board->mGameScene->GetCurrentWeatherNoticeTimer() : 0.0f;
	j["weatherForecastFailureTimer"] = board->mGameScene
		? board->mGameScene->GetWeatherForecastFailureTimer() : 0.0f;
	j["failedForecastRainIntensity"] = board->mGameScene
		? static_cast<int>(board->mGameScene->GetFailedForecastRainIntensity())
		: static_cast<int>(RainIntensity::CLEAR);
	j["weatherForecastFailureActualIntensity"] = board->mGameScene
		? static_cast<int>(board->mGameScene->GetActualForecastRainIntensity())
		: static_cast<int>(RainIntensity::CLEAR);
	j["maxWave"] = board->mMaxWave;
	j["zombieCountDown"] = board->mZombieCountDown;
	j["totalZombieHP"] = board->mTotalZombieHP;
	j["currentWaveZombieHP"] = board->mCurrectWaveZombieHP;
	j["nextWaveSpawnZombieHP"] = board->mNextWaveSpawnZombieHP;

	// 保存 EntityManager 的 ID 计数器
	j["nextPlantID"] = board->mEntityManager.GetNextPlantID();
	j["nextZombieID"] = board->mEntityManager.GetNextZombieID();
	j["nextBulletID"] = board->mEntityManager.GetNextBulletID();
	j["nextCoinID"] = board->mEntityManager.GetNextCoinID();
	j["nextMowerID"] = board->mEntityManager.GetNextMowerID();

	// 植物
	nlohmann::json plantsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllPlantIDs()) {
		auto plant = board->mEntityManager.GetPlant(id);
		if (!plant) continue;
		nlohmann::json p;
		p["id"] = id;
		p["type"] = static_cast<int>(plant->mPlantType);
		// 阵风触发时 row/column 已先落到目标格；纯视觉追赶偏移不保存，读档稳定吸附目标格。
		p["row"] = plant->mRow;
		p["column"] = plant->mColumn;
		p["health"] = plant->mPlantHealth;
		p["maxHealth"] = plant->mPlantMaxHealth;
		p["isSleeping"] = plant->GetSleepState();
		SaveAnimState(p, plant);

		nlohmann::json extraData;
		plant->SaveExtraData(extraData);
		if (!extraData.empty()) {
			p["extraData"] = extraData;
		}
		plantsArr.push_back(p);
	}
	j["plants"] = plantsArr;

	// 小推车
	nlohmann::json mowersArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllMowerIDs()) {
		auto* mower = board->mEntityManager.GetMower(id);
		if (!mower) continue;
		nlohmann::json m;
		m["id"] = id;
		m["type"] = static_cast<int>(mower->mMowerType);
		m["row"] = mower->mRow;
		m["state"] = static_cast<int>(mower->mState);
		m["speed"] = mower->mSpeed;
		m["x"] = mower->GetPosition().x;
		m["y"] = mower->GetPosition().y;
		SaveAnimState(m, mower);
		mowersArr.push_back(m);
	}
	j["mowers"] = mowersArr;

	// 僵尸
	nlohmann::json zombiesArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllZombieIDs()) {
		auto zombie = board->mEntityManager.GetZombie(id);
		// 濒死僵尸 Die() 后 shared_ptr 仍滞留在 GameObjectManager 的待删队列中（要到下一帧 flush 才释放），
		// 其 weak_ptr 此刻仍可 lock，会被 GetAllZombieIDs 返回。mActive 已在 Die() 置 false，借此排除，
		// 避免把触发轮清的那只死尸序列化进存档（否则重载会复活成血量≤0 的幽灵僵尸）。
		if (!zombie || !zombie->IsActive()) continue;
		nlohmann::json z;
		z["id"] = id;
		z["type"] = static_cast<int>(zombie->mZombieType);
		z["row"] = zombie->mRow;
		z["x"] = zombie->GetPosition().x;

		z["bodyHealth"] = zombie->mBodyHealth;
		z["bodyMaxHealth"] = zombie->mBodyMaxHealth;
		z["helmType"] = zombie->mHelmType;
		z["helmHealth"] = zombie->mHelmHealth;
		z["helmMaxHealth"] = zombie->mHelmMaxHealth;
		z["shieldType"] = zombie->mShieldType;
		z["shieldHealth"] = zombie->mShieldHealth;
		z["shieldMaxHealth"] = zombie->mShieldMaxHealth;

		z["spawnWave"] = zombie->mSpawnWave;
		z["attackDamage"] = zombie->mAttackDamage;
		z["needDropArm"] = zombie->mNeedDropArm;
		z["needDropHead"] = zombie->mNeedDropHead;
		zombie->SaveProtectedData(z);
		SaveAnimState(z, zombie);
		nlohmann::json extraData;
		zombie->SaveExtraData(extraData);
		if (!extraData.empty()) {
			z["extraData"] = extraData;
		}
		zombiesArr.push_back(z);
	}
	j["zombies"] = zombiesArr;

	// 子弹
	nlohmann::json bulletsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllBulletIDs()) {
		auto bullet = board->mEntityManager.GetBullet(id);
		if (!bullet) continue;
		nlohmann::json b;
		b["type"] = static_cast<int>(bullet->mBulletType);
		b["id"] = id;
		b["row"] = bullet->mRow;
		b["x"] = bullet->GetPosition().x;
		b["y"] = bullet->GetPosition().y;
		b["damage"] = bullet->GetBulletDamage();
		b["velocityX"] = bullet->GetVelocityX();
		b["velocityY"] = bullet->GetVelocityY();
		bulletsArr.push_back(b);
	}
	j["bullets"] = bulletsArr;

	// 太阳
	nlohmann::json sunsArr = nlohmann::json::array();
	for (int id : board->mEntityManager.GetAllCoinIDs()) {
		auto* coin = board->mEntityManager.GetCoin(id);
		if (!coin) continue;
		auto* sun = dynamic_cast<Sun*>(coin);
		if (sun) {
			nlohmann::json s;
			s["id"] = id;
			s["x"] = sun->GetPosition().x;
			s["y"] = sun->GetPosition().y;
			SaveAnimState(s, sun);
			s["small"] = (dynamic_cast<SmallSun*>(coin) != nullptr);	// 区分大/小阳光
			sunsArr.push_back(s);
		}
	}
	j["suns"] = sunsArr;

	// 奖杯（每关至多一个，Board 直接持有引用，不走金币表）
	nlohmann::json trophiesArr = nlohmann::json::array();
	if (auto trophy = board->mTrophy.lock()) {
		nlohmann::json t;
		t["x"] = trophy->GetPosition().x;
		t["y"] = trophy->GetPosition().y;
		trophiesArr.push_back(t);
	}
	j["trophies"] = trophiesArr;

	// 弹坑（毁灭菇）：row/col/剩余秒数即可完整还原；无弹坑时省略字段，旧档天然兼容
	nlohmann::json cratersArr = nlohmann::json::array();
	for (auto& weak : board->mCraters) {
		auto crater = weak.lock();
		if (!crater || !crater->IsActive()) continue;
		cratersArr.push_back({
			{ "row", crater->mRow },
			{ "column", crater->mColumn },
			{ "timeLeft", crater->mTimeLeft },
		});
	}
	if (!cratersArr.empty()) j["craters"] = cratersArr;

	// 卡牌只在 GAME 中代表已提交卡组。CHOOSE_CARD 中的卡槽必须为空；即便未来流程意外留下
	// 旧卡，也不能把它序列化成下一轮已提交卡组，否则读档后再次选卡会叠出两套卡牌。
	nlohmann::json cardsArr = nlohmann::json::array();
	if (manager && board->mBoardState == BoardState::GAME) {
		for (auto* card : manager->GetCards()) {
			if (!card) continue;
			auto comp = card->GetComponent<CardComponent>();
			if (!comp) continue;
			auto transform = card->GetComponent<TransformComponent>();
			if (!transform) continue;
			nlohmann::json c;
			c["plantType"] = static_cast<int>(comp->GetPlantType());
			c["posX"] = transform->GetPosition().x;
			c["posY"] = transform->GetPosition().y;
			c["sunCost"] = comp->GetSunCost();
			c["cooldownTime"] = comp->GetCooldownTime();
			c["isCooldown"] = comp->IsCooldown();
			c["cooldownTimer"] = comp->GetCooldownTimer();
			cardsArr.push_back(c);
		}
	}
	j["cards"] = cardsArr;

	// 生存轮间空槽重选的冷却快照：选卡阶段卡槽已被清空，仍在冷却的卡牌冷却进度暂存在 GameScene 的
	// mSurvivalCardCooldowns 里（而非卡槽）。它是该进度的唯一载体，必须单独持久化，否则选卡界面
	// 退出重进后快照丢失 → 选完卡冷却被清零。仅 survival 写入，普通模式无此字段、行为不变。
	if (board->mIsSurvival && board->mGameScene) {
		nlohmann::json cooldownArr = nlohmann::json::array();
		for (const auto& [type, cd] : board->mGameScene->GetSurvivalCardCooldowns()) {
			nlohmann::json c;
			c["plantType"] = static_cast<int>(type);
			c["cooldownTimer"] = cd.first;
			c["cooldownTime"] = cd.second;
			cooldownArr.push_back(c);
		}
		j["survivalCardCooldowns"] = cooldownArr;
	}

	std::string filename = GetSaveRoot() + "/level" + std::to_string(board->mLevel) + "_data.json";
	return FileManager::SaveJsonFile(filename, j);
}

bool GameInfoSaver::LoadLevelDataImpl(Board* board, CardSlotManager* manager)
{
	// AutoTest 默认仍是确定性的全新关卡；仅显式 -AutoTestLoadSave 时读取当前 CWD 下的
	// 关卡存档。写入和删除入口始终短路，因此问题存档在测试后保持逐字节不变。
	if (GameAPP::mAutoTestMode && !GameAPP::mAutoTestLoadSave) return true;
	std::string filename = GetSaveRoot() + "/level" + std::to_string(board->mLevel) + "_data.json";
	nlohmann::json j;
	if (!FileManager::LoadJsonFile(filename, j))
		return false;

	board->mIsLoadSave = true;		// 读档标记

	// 恢复 Board 状态
	board->mBoardState = static_cast<BoardState>(j.value("boardState", static_cast<int>(BoardState::GAME)));
	board->mIsSurvival = j.value("isSurvival", false);
	board->mSurvivalRound = j.value("survivalRound", 1);
	if (board->mIsSurvival) {
		if (j.contains("perks")) board->GetPerkManager().Load(j["perks"]);   // 旧档无 perks 字段→天然兼容

		// 还原冻结的出怪池（防刷怪）：存档若带 spawnList 字段则直接还原（校验+去重，镜像
		// Board::LoadSpawnListFromJson 的健壮性，挡住手改/损坏档的越界 ZombieType）；旧档无此
		// 字段→回退到按轮次重建（旧行为，天然兼容），重建后下次存档即写入字段冻结。
		if (j.contains("spawnList") && j["spawnList"].is_array() && !j["spawnList"].empty()) {
			std::vector<ZombieType> list;
			for (auto& v : j["spawnList"]) {
				int val = v.get<int>();
				if (val < 0 || val >= static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES)) continue;
				bool dup = false;
				for (ZombieType seen : list)
					if (static_cast<int>(seen) == val) { dup = true; break; }
				if (dup) continue;
				list.push_back(static_cast<ZombieType>(val));
			}
			if (!list.empty()) board->SetZombieSpawnList(list);                   // 还原冻结池
			else board->BuildSurvivalSpawnList(board->mSurvivalRound);            // 全损坏→兜底重建
		} else {
			board->BuildSurvivalSpawnList(board->mSurvivalRound);                // 旧档/无字段→旧行为
		}
		board->UpdateSurvivalLevelName();
		// 轮间（CHOOSE_CARD）读档：Board 构造时按第1轮建的预览僵尸阵容是错的，
		// 此刻 survivalRound/出怪表已恢复，销毁旧预览并按正确轮次重建。
		// （GAME 状态读档不处理：OnEnter 会走 StartGame 销毁预览）
		if (board->mBoardState == BoardState::CHOOSE_CARD) {
			board->DestroyPreviewZombies();
			board->CreatePreviewZombies();
		}
	}
	board->mSun = j.value("sun", 50);
	board->mCurrentWave = j.value("currentWave", 0);
	board->mBoardFrame = j.value("boardFrame", 0);
	board->mWeatherInitialized = j.value("weatherInitialized", j.contains("rainIntensity"));
	const int rainValue = j.value("rainIntensity", static_cast<int>(RainIntensity::CLEAR));
	board->mRainIntensity = (rainValue >= static_cast<int>(RainIntensity::CLEAR)
		&& rainValue <= static_cast<int>(RainIntensity::HEAVY))
		? static_cast<RainIntensity>(rainValue) : RainIntensity::CLEAR;
	const int previousRainValue = j.value("previousRainIntensity", rainValue);
	const bool validPreviousRain = previousRainValue >= static_cast<int>(RainIntensity::CLEAR)
		&& previousRainValue <= static_cast<int>(RainIntensity::HEAVY);
	// 旧档没有过渡字段时直接落在目标天气；损坏枚举也按目标天气稳定恢复。
	board->RestoreWeatherTransition(
		validPreviousRain ? static_cast<RainIntensity>(previousRainValue) : board->mRainIntensity,
		validPreviousRain ? j.value("weatherTransitionTimer", 0.0f) : 0.0f);
	const int forecastRainValue = j.value("forecastRainIntensity",
		static_cast<int>(RainIntensity::CLEAR));
	const bool validForecastRain = forecastRainValue >= static_cast<int>(RainIntensity::CLEAR)
		&& forecastRainValue <= static_cast<int>(RainIntensity::HEAVY);
	const int actualForecastRainValue = j.value("actualForecastRainIntensity",
		static_cast<int>(RainIntensity::CLEAR));
	const bool validActualForecastRain = actualForecastRainValue >= static_cast<int>(RainIntensity::CLEAR)
		&& actualForecastRainValue <= static_cast<int>(RainIntensity::HEAVY);
	board->mWeatherForecastReady = j.value("weatherForecastReady", false)
		&& j.contains("actualForecastRainIntensity")
		&& validForecastRain && validActualForecastRain;
	board->mForecastRainIntensity = board->mWeatherForecastReady
		? static_cast<RainIntensity>(forecastRainValue) : RainIntensity::CLEAR;
	board->mActualForecastRainIntensity = board->mWeatherForecastReady
		? static_cast<RainIntensity>(actualForecastRainValue) : RainIntensity::CLEAR;
	if (board->mGameScene) {
		// 缺字段的旧档按 0 秒恢复，避免读入雨中存档时把已消失的展板重新显示 5 秒。
		board->mGameScene->RestoreCurrentWeatherNotice(
			j.value("currentWeatherNoticeTimer", 0.0f));

		const int failedForecastRainValue = j.value("failedForecastRainIntensity",
			static_cast<int>(RainIntensity::CLEAR));
		const int failureActualRainValue = j.value("weatherForecastFailureActualIntensity",
			static_cast<int>(RainIntensity::CLEAR));
		const bool validFailedForecastRain = failedForecastRainValue >= static_cast<int>(RainIntensity::CLEAR)
			&& failedForecastRainValue <= static_cast<int>(RainIntensity::HEAVY);
		const bool validFailureActualRain = failureActualRainValue >= static_cast<int>(RainIntensity::CLEAR)
			&& failureActualRainValue <= static_cast<int>(RainIntensity::HEAVY);
		// 旧档或损坏字段按 0 秒恢复，已经消失的失败提示不会在读档后重播。
		board->mGameScene->RestoreWeatherForecastFailure(
			validFailedForecastRain && validFailureActualRain
				? j.value("weatherForecastFailureTimer", 0.0f) : 0.0f,
			validFailedForecastRain ? static_cast<RainIntensity>(failedForecastRainValue) : RainIntensity::CLEAR,
			validFailureActualRain ? static_cast<RainIntensity>(failureActualRainValue) : RainIntensity::CLEAR);
	}
	board->mWeatherTimer = std::max(0.0f, j.value("weatherTimer", 0.0f));
	board->mLightningTimer = std::max(0.0f, j.value("lightningTimer", 0.0f));
	// 旧版天气存档没有该字段时按 false：少一次增强机会比读档后凭空再增强更稳妥。
	board->mRainCanIntensify = board->mRainIntensity == RainIntensity::LIGHT
		&& j.value("rainCanIntensify", false);
	// 旧档没有续期字段时按已消费处理；小雨永远不开放同档续期。
	board->mRainCanHold = (board->mRainIntensity == RainIntensity::MEDIUM
		|| board->mRainIntensity == RainIntensity::HEAVY)
		&& j.value("rainCanHold", false);
	const int typhoonValue = j.value("typhoonStrength",
		static_cast<int>(TyphoonStrength::NONE));
	const int windDirectionValue = j.value("windDirection",
		static_cast<int>(WindDirection::NONE));
	const TyphoonStrength typhoonStrength = typhoonValue >= static_cast<int>(TyphoonStrength::NONE)
		&& typhoonValue <= static_cast<int>(TyphoonStrength::SUPER)
		? static_cast<TyphoonStrength>(typhoonValue) : TyphoonStrength::NONE;
	const WindDirection windDirection = windDirectionValue >= static_cast<int>(WindDirection::NONE)
		&& windDirectionValue <= static_cast<int>(WindDirection::TOWARD_FRONT)
		? static_cast<WindDirection>(windDirectionValue) : WindDirection::NONE;
	// 台风强度、风向和计时都是已经判定过的结果；旧档或损坏组合只退化为无台风，绝不重 roll。
	board->RestoreTyphoonState(typhoonStrength, windDirection,
		j.value("typhoonStrengthTimer", 0.0f), j.value("windGustTimer", 0.0f),
		j.value("windDirectionTimer", 0.0f),
		j.value("typhoonGustsRemaining", 0));
	const int activeGustStrengthValue = j.value("activeGustStrength",
		static_cast<int>(TyphoonStrength::NONE));
	const int activeGustDirectionValue = j.value("activeGustDirection",
		static_cast<int>(WindDirection::NONE));
	const TyphoonStrength activeGustStrength = activeGustStrengthValue
		>= static_cast<int>(TyphoonStrength::NONE)
		&& activeGustStrengthValue <= static_cast<int>(TyphoonStrength::SUPER)
		? static_cast<TyphoonStrength>(activeGustStrengthValue) : TyphoonStrength::NONE;
	const WindDirection activeGustDirection = activeGustDirectionValue
		>= static_cast<int>(WindDirection::NONE)
		&& activeGustDirectionValue <= static_cast<int>(WindDirection::TOWARD_FRONT)
		? static_cast<WindDirection>(activeGustDirectionValue) : WindDirection::NONE;
	// 活动阵风的锁定值、余时和植物结算标记必须入档，否则会中途停风或重复换格。
	board->RestoreActiveTyphoonGust(j.value("typhoonGustActive", false),
		activeGustStrength, activeGustDirection,
		j.value("activeGustDuration", 0.0f), j.value("activeGustTimer", 0.0f),
		j.value("activeGustPlantMoveTimer", 0.0f),
		j.value("activeGustPlantMoved", false));
	// 保底计数影响下一次大雨的概率，必须随档恢复；旧档默认从零开始。
	board->RestoreTyphoonPity(j.value("heavyPhasesWithoutTyphoon", 0));
	board->mRainVisualActive = false;   // 粒子不入存档，StartGame 按剩余时间重建
	board->mMaxWave = j.value("maxWave", 10);
	board->mZombieCountDown = j.value("zombieCountDown", 20.0f);
	board->mTotalZombieHP = j.value("totalZombieHP", 0LL);
	board->mCurrectWaveZombieHP = j.value("currentWaveZombieHP", 0LL);
	board->mNextWaveSpawnZombieHP = j.value("nextWaveSpawnZombieHP", 0LL);

	// 恢复 EntityManager 的 ID 计数器（向后兼容：旧存档没有则使用默认值）
	board->mEntityManager.SetNextPlantID(j.value("nextPlantID", 1));
	board->mEntityManager.SetNextZombieID(j.value("nextZombieID", 1));
	board->mEntityManager.SetNextBulletID(j.value("nextBulletID", 1));
	board->mEntityManager.SetNextCoinID(j.value("nextCoinID", 1));
	board->mEntityManager.SetNextMowerID(j.value("nextMowerID", 1));

	// 恢复进度条
	if (board->mCurrentWave > 0) {
		auto gameProgress = board->mGameScene->GetGameProgress();
		gameProgress->SetActive(true);
		auto& res = ResourceManager::GetInstance();
		gameProgress->SetupFlags(res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_STICK)
			, res.GetTexture(ResourceKeys::Textures::IMAGE_FLAGMETER_PART_FLAG));
	}

	// 恢复植物
	for (auto& p : j.value("plants", nlohmann::json::array())) {
		PlantType type = static_cast<PlantType>(p["type"].get<int>());
		int row = p["row"].get<int>();
		int col = p["column"].get<int>();
		int health = p["health"].get<int>();
		int maxHealth = p["maxHealth"].get<int>();
		bool isSleeping = p["isSleeping"].get<bool>();
		int id = p.value("id", NULL_PLANT_ID);

		Plant* plant = nullptr;
		if (id != NULL_PLANT_ID) {
			plant = board->CreatePlantWithID(type, row, col, id);
		}
		else {
			plant = board->CreatePlant(type, row, col);
		}

		if (plant) {
			plant->mPlantHealth = health;
			plant->mPlantMaxHealth = maxHealth;
			plant->SetSleepState(isSleeping);
			RestoreAnimState(p, plant);
			if (p.contains("extraData")) {
				plant->LoadExtraData(p["extraData"]);
			}
		}
	}

	// 恢复小推车
	for (auto& m : j.value("mowers", nlohmann::json::array())) {
		MowerType type = static_cast<MowerType>(m["type"].get<int>());
		int row = m["row"].get<int>();
		float x = m["x"].get<float>();
		float y = m["y"].get<float>();
		int id = m.value("id", NULL_MOWER_ID);

		Mower* mower = nullptr;
		if (id != NULL_MOWER_ID) {
			mower = board->CreateMowerWithID(type, row, x, y, id);
		}
		else {
			mower = board->CreateMower(type, row);
		}

		if (mower) {
			mower->mState = static_cast<MowerState>(m.value("state", 0));
			mower->mSpeed = m.value("speed", 300.0f);
			RestoreAnimState(m, mower);
		}
	}

	// 恢复僵尸
	for (auto& z : j.value("zombies", nlohmann::json::array())) {
		ZombieType type = static_cast<ZombieType>(z["type"].get<int>());
		int   row = z["row"].get<int>();
		float x = z["x"].get<float>();
		int   id = z.value("id", NULL_ZOMBIE_ID);

		Zombie* zombie = nullptr;
		if (id != NULL_ZOMBIE_ID) {
			zombie = board->CreateZombieWithID(type, row, x, id);
		}
		else {
			zombie = board->CreateZombie(type, row, x);
		}

		if (zombie) {
			zombie->mBodyHealth = z.value("bodyHealth", 270);
			zombie->mBodyMaxHealth = z.value("bodyMaxHealth", 270);
			zombie->mHelmType = static_cast<HelmType>(z.value("helmType", HelmType::HELMTYPE_NONE));
			zombie->mHelmHealth = z.value("helmHealth", 0);
			zombie->mHelmMaxHealth = z.value("helmMaxHealth", 0);
			zombie->mShieldType = static_cast<ShieldType>(z.value("shieldType", ShieldType::SHIELDTYPE_NONE));
			zombie->mShieldHealth = z.value("shieldHealth", 0);
			zombie->mShieldMaxHealth = z.value("shieldMaxHealth", 0);
			zombie->mSpawnWave = z.value("spawnWave", -1);
			zombie->mAttackDamage = z.value("attackDamage", 50);
			zombie->mNeedDropArm = z.value("needDropArm", true);
			zombie->mNeedDropHead = z.value("needDropHead", true);

			zombie->LoadProtectedData(z);
			RestoreAnimState(z, zombie);

			if (z.contains("extraData")) {
				zombie->LoadExtraData(z["extraData"]);
			}

			zombie->ZombieItemUpdate();
		}
	}

	// 验证僵尸进食状态（防止植物不存在时崩溃）
	for (int id : board->mEntityManager.GetAllZombieIDs()) {
		auto zombie = board->mEntityManager.GetZombie(id);
		if (!zombie) continue;
		zombie->ValidateEatingState(board->mEntityManager);
	}

	// 恢复子弹
	for (auto& b : j.value("bullets", nlohmann::json::array())) {
		BulletType type = static_cast<BulletType>(b["type"].get<int>());
		int   row = b["row"].get<int>();
		float x = b["x"].get<float>();
		float y = b["y"].get<float>();
		int   id = b.value("id", NULL_BULLET_ID);

		Bullet* bullet = nullptr;
		if (id != NULL_BULLET_ID) {
			bullet = board->CreateBulletWithID(type, row, Vector(x, y), id);
		}
		else {
			bullet = board->CreateBullet(type, row, Vector(x, y));
		}

		if (bullet) {
			bullet->mFromPool = false;
			bullet->SetBulletDamage(b["damage"].get<int>());
			bullet->SetVelocityX(b["velocityX"].get<float>());
			bullet->SetVelocityY(b["velocityY"].get<float>());
		}
	}

	// 恢复太阳
	for (auto& s : j.value("suns", nlohmann::json::array())) {
		float x = s["x"].get<float>();
		float y = s["y"].get<float>();
		int  id = s.value("id", NULL_COIN_ID);
		bool small = s.value("small", false);	// 旧存档无此字段→普通阳光，向后兼容

		Sun* sun = nullptr;
		if (id != NULL_COIN_ID) {
			sun = small ? static_cast<Sun*>(board->CreateSmallSunWithID(Vector(x, y), false, id))
						: board->CreateSunWithID(Vector(x, y), false, id);
		}
		else {
			sun = small ? static_cast<Sun*>(board->CreateSmallSun(Vector(x, y), false))
						: board->CreateSun(Vector(x, y), false);
		}

		if (sun) {
			RestoreAnimState(s, sun);
		}
	}

	// 恢复奖杯（旧存档带 "id" 字段，已不再使用，直接忽略）
	for (auto& t : j.value("trophies", nlohmann::json::array())) {
		float x = t["x"].get<float>();
		float y = t["y"].get<float>();
		board->CreateTrophy(Vector(x, y));
	}

	// 恢复弹坑（毁灭菇）；旧档无 craters 字段 → 空数组，天然兼容
	for (auto& c : j.value("craters", nlohmann::json::array())) {
		board->AddCrater(c.value("row", 0), c.value("column", 0),
			c.value("timeLeft", Crater::CRATER_DURATION));
	}

	// CHOOSE_CARD 存档中的 cards 来自旧版词条选择退出 bug：它们是上一轮卡组，不是下一轮
	// 已提交选择。此时禁止恢复到卡槽；若显式冷却快照为空，则只迁移其中仍在冷却的进度。
	const bool isSurvivalCardSelect = board->mIsSurvival
		&& board->mBoardState == BoardState::CHOOSE_CARD;
	std::unordered_map<PlantType, std::pair<float, float>> legacyCardCooldowns;
	for (auto& c : j.value("cards", nlohmann::json::array())) {
		if (isSurvivalCardSelect) {
			if (c.value("isCooldown", false)) {
				PlantType type = static_cast<PlantType>(c["plantType"].get<int>());
				legacyCardCooldowns[type] = {
					c.value("cooldownTimer", 0.0f), c.value("cooldownTime", 0.0f)
				};
			}
			continue;
		}
		if (manager) {
			PlantType plantType = static_cast<PlantType>(c["plantType"].get<int>());
			float posX = c["posX"].get<float>();
			float posY = c["posY"].get<float>();
			int sunCost = c["sunCost"].get<int>();
			float cooldownTime = c["cooldownTime"].get<float>();
			bool isCooldown = c["isCooldown"].get<bool>();
			float cooldownTimer = c["cooldownTimer"].get<float>();

			auto card = GameObjectManager::GetInstance().CreateGameObjectImmediate<Card>(
				LAYER_UI, plantType, sunCost, cooldownTime, false);
			if (!card) continue;

			if (auto transform = card->GetComponent<TransformComponent>()) {
				transform->SetPosition(Vector(posX, posY));
			}
			if (isCooldown) {
				if (auto comp = card->GetComponent<CardComponent>()) {
					comp->RestoreCooldown(cooldownTimer, cooldownTime);
				}
			}
			manager->AddCard(card);
		}
	}

	// 恢复生存轮间冷却快照（见 SaveLevelData 同名字段注释）。必须在 ChooseCardComplete 还原冷却之前就位，
	// 而本函数在 OnEnter 选卡分支之前执行，时序成立。旧版问题档的显式快照为空时，从被丢弃的
	// 上一轮 cards 迁移冷却数据，既阻止重复卡牌，也不损失原本仍在冷却的进度。
	if (board->mIsSurvival && board->mGameScene) {
		std::unordered_map<PlantType, std::pair<float, float>> cooldowns;
		if (j.contains("survivalCardCooldowns") && j["survivalCardCooldowns"].is_array()) {
			for (auto& c : j["survivalCardCooldowns"]) {
				PlantType type = static_cast<PlantType>(c["plantType"].get<int>());
				cooldowns[type] = { c.value("cooldownTimer", 0.0f), c.value("cooldownTime", 0.0f) };
			}
		}
		if (cooldowns.empty() && isSurvivalCardSelect)
			cooldowns = std::move(legacyCardCooldowns);
		board->mGameScene->SetSurvivalCardCooldowns(std::move(cooldowns));
	}

	// 恢复旗子升起状态，并立刻对齐进度条滑块（跳过缓动动画）
	if (board->mGameScene) {
		if (auto progress = board->mGameScene->GetGameProgress()) {
			progress->InitializeRaisedFlags(-10.0f);
			progress->SnapProgressToCurrentWave();
		}
	}

	return true;
}

bool GameInfoSaver::DeleteLevelData(Board* board)
{
	if (GameAPP::mAutoTestMode) return true;   // AutoTest（包括读档复现模式）绝不删除真实存档
	std::string filename = GetSaveRoot() + "/level" + std::to_string(board->mLevel) + "_data.json";
	return FileManager::DeleteFile(filename);
}

// ── 异常安全边界 ──────────────────────────────────────────────────────────────
// 公有存档/读档接口统一包一层 try/catch：任何序列化异常（nlohmann 的 type_error/
// parse_error、缺键、类型不符等，均派生自 std::exception）都转成返回 false（沿用既有
// “失败”契约），不再逸出到 CrashHandler 直接崩坏整个游戏。
// 注意：LoadLevelData 中途抛异常时 Board 可能已部分加载（mIsLoadSave/已建实体不会回滚），
// 返回 false 仅保证“不崩”；调用方应把 false 视为读档失败、按全新关卡处理。
bool GameInfoSaver::SavePlayerInfo()
{
	try { return SavePlayerInfoImpl(); }
	catch (const std::exception& e) {
		LOG_ERROR("GameInfoSaver") << "SavePlayerInfo 异常，已跳过保存: " << e.what();
		return false;
	}
}

bool GameInfoSaver::LoadPlayerInfo()
{
	try { return LoadPlayerInfoImpl(); }
	catch (const std::exception& e) {
		LOG_ERROR("GameInfoSaver") << "LoadPlayerInfo 异常，按默认设置继续: " << e.what();
		return false;
	}
}

bool GameInfoSaver::SaveLevelData(Board* board, CardSlotManager* manager)
{
	try { return SaveLevelDataImpl(board, manager); }
	catch (const std::exception& e) {
		LOG_ERROR("GameInfoSaver") << "SaveLevelData 异常，已跳过保存: " << e.what();
		return false;
	}
}

bool GameInfoSaver::LoadLevelData(Board* board, CardSlotManager* manager)
{
	try { return LoadLevelDataImpl(board, manager); }
	catch (const std::exception& e) {
		LOG_ERROR("GameInfoSaver") << "LoadLevelData 异常，已放弃读档（关卡将重新开始）: " << e.what();
		return false;
	}
}
