#include "GameInfoSaver.h"
#include "GameApp.h"
#include "./Game/AudioSystem.h"

bool GameInfoSaver::SavePlayerInfo()
{
	auto& gameApp = GameAPP::GetInstance();

	FileManager::CreateDirectory("./save");

	nlohmann::json j;
	j["difficulty"] = gameApp.Difficulty;
	j["adventureLevel"] = gameApp.mAdventureLevel;
	j["showPlantHP"] = gameApp.mShowPlantHP;
	j["showZombieHP"] = gameApp.mShowZombieHP;
	j["autoCollected"] = gameApp.mAutoCollected;
	j["soundVolume"] = AudioSystem::GetSoundVolume();
	j["musicVolume"] = AudioSystem::GetMusicVolume();

	return FileManager::SaveJsonFile("./save/PlayerInfo.json", j);
}

bool GameInfoSaver::LoadPlayerInfo()
{
	nlohmann::json j;
	if (!FileManager::LoadJsonFile("./save/PlayerInfo.json", j))
		return false;

	auto& gameApp = GameAPP::GetInstance();
	gameApp.Difficulty = j.value("difficulty", 1);
	gameApp.mAdventureLevel = j.value("adventureLevel", 1);
	gameApp.mShowPlantHP = j.value("showPlantHP", false);
	gameApp.mShowZombieHP = j.value("showZombieHP", false);
	gameApp.mAutoCollected = j.value("autoCollected", true);

	AudioSystem::SetSoundVolume(j.value("soundVolume", 0.5f));
	AudioSystem::SetMusicVolume(j.value("musicVolume", 0.5f));

	return true;
}

bool GameInfoSaver::SaveLevelData(Board* /*board*/)
{
	return false;
}

bool GameInfoSaver::LoadLevelData(Board* /*board*/)
{
	return false;
}
