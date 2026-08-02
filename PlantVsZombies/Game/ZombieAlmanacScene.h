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
	std::vector<ZombieType> mDisplayedZombieTypes;
	std::weak_ptr<Zombie> mPreviewZombie;

	ZombieType mCurrentZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	std::unordered_map<std::string, std::string> mInfoMap;
	std::string mCurrentZombieName;
	float mZombieNameX = 0.0f;
	std::vector<std::string> mDescriptionLines;
	int   mDescriptionFontSize = 17;     // 自动收缩后的描述字号
	float mDescriptionLineHeight = 22.0f; // 与字号等比的行高

	/** 合并已通关 spawnlist 与永久实际遭遇记录，按首次推导顺序返回可见僵尸。 */
	std::vector<ZombieType> LoadEncounteredZombieTypes() const;
	/** 为已遭遇类型创建可点击的图鉴网格和裁剪预览。 */
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

	/** 返回当前进度下实际展示的僵尸类型，顺序为冒险模式首次遭遇顺序。 */
	const std::vector<ZombieType>& GetDisplayedZombieTypes() const {
		return mDisplayedZombieTypes;
	}

	/** 返回右侧详情当前选中的僵尸；空图鉴返回 NUM_ZOMBIE_TYPES。 */
	ZombieType GetCurrentZombieType() const { return mCurrentZombieType; }
	/** 返回右侧详情预览实例；仅供只读状态取证。 */
	Zombie* GetPreviewZombie() const {
		const auto zombie = mPreviewZombie.lock();
		return zombie.get();
	}

	bool mReadyToSwitchAlmanacScene = false;

protected:
	void BuildDrawCommands() override;
};

#endif
