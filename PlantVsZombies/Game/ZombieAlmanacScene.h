#pragma once
#ifndef _ZOMBIEALMANAC_SCENE_H
#define _ZOMBIEALMANAC_SCENE_H

#include "Scene.h"
#include "Zombie/ZombieType.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <SDL2/SDL_ttf.h>

class Zombie;

class ZombieAlmanacScene : public Scene {
private:
	std::shared_ptr<Button> mBackMenuButton;
	std::vector<std::weak_ptr<Zombie>> mGridZombies;
	std::vector<Vector> mGridPositions;
	std::weak_ptr<Zombie> mPreviewZombie;

	ZombieType mCurrentZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	std::unordered_map<std::string, std::string> mInfoMap;
	std::string mCurrentZombieName;
	float mZombieNameX = 0.0f;
	std::vector<std::string> mDescriptionLines;

	void CreateAllZombieEntries();
	void OnZombieClicked(ZombieType type);
	void CreatePreviewZombie(ZombieType type);
	void DestroyPreviewZombie();
	void LoadInfoFile();
	void UpdateZombieInfo(ZombieType type);
	std::vector<std::string> WrapText(const std::string& text,
		float startX, float maxX, float wrapX,
		const std::string& fontKey, int fontSize);

public:
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	bool mReadyToSwitchAlmanacScene = false;

protected:
	void BuildDrawCommands() override;
};

#endif
