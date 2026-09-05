#pragma once
#ifndef _PLANTALMANAC_SCENE_H
#define _PLANTALMANAC_SCENE_H

#include "Scene.h"
#include "Card.h"
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <SDL2/SDL_ttf.h>

class Plant;

class PlantAlmanacScene : public Scene {
private:
	PlantType mRewardPlant;
	bool mReadyForNextLevel = false;
	float GetInfoOffsetX() const { return IsReward() ? -357.0f : 0.0f; }
	std::shared_ptr<Button> mBackMenuButton;
	std::vector<Card*> mCards;
	std::weak_ptr<Plant> mPreviewPlant;

	PlantType mCurrentPlantType = PlantType::NUM_PLANT_TYPES;
	std::unordered_map<std::string, std::string> mInfoMap;
	std::string mCurrentPlantName;
	float mPlantNameX = 0.0f;
	std::vector<std::string> mDescriptionLines;
	int   mDescriptionFontSize = 17;     // 自动收缩后的描述字号
	float mDescriptionLineHeight = 22.0f; // 与字号等比的行高

	void CreateAllCards();
	void OnCardClicked(PlantType type);
	void CreatePreviewPlant(PlantType type);
	void DestroyPreviewPlant();
	void LoadInfoFile();
	void UpdatePlantInfo(PlantType type);
	std::vector<std::string> WrapText(const std::string& text,
		float startX, float maxX, float wrapX,
		const std::string& fontKey, int fontSize);

public:
	/** 无参数为普通图鉴；指定植物为通关奖励页，共用资料与预览。 */
	explicit PlantAlmanacScene(PlantType reward = PlantType::NUM_PLANT_TYPES)
		: mRewardPlant(reward) {}
	bool IsReward() const { return mRewardPlant != PlantType::NUM_PLANT_TYPES; }
	PlantType GetSelectedPlant() const { return mCurrentPlantType; }
	const std::string& GetPlantName() const { return mCurrentPlantName; }
	size_t GetDescriptionLineCount() const { return mDescriptionLines.size(); }
	bool HasPreviewPlant() const { return !mPreviewPlant.expired(); }
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	bool mReadyToSwitchAlmanacScene = false;

protected:
	void BuildDrawCommands() override;
};

#endif
