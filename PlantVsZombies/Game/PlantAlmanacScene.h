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
	std::shared_ptr<Button> mBackMenuButton;
	std::vector<Card*> mCards;
	std::weak_ptr<Plant> mPreviewPlant;

	PlantType mCurrentPlantType = PlantType::NUM_PLANT_TYPES;
	std::unordered_map<std::string, std::string> mInfoMap;
	std::string mCurrentPlantName;
	float mPlantNameX = 0.0f;
	std::vector<std::string> mDescriptionLines;

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
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

	bool mReadyToSwitchAlmanacScene = false;

protected:
	void BuildDrawCommands() override;
};

#endif