#include "PlantAlmanacScene.h"
#include "SceneManager.h"
#include "../GameApp.h"
#include "Plant/Plant.h"
#include "ClickableComponent.h"
#include "./Plant/GameDataManager.h"
#include "GameObjectManager.h"
#include <fstream>
#include <algorithm>

constexpr float CARD_INIT_POSITION_X = 70.0f;
constexpr float CARD_INIT_POSITION_Y = 120.0f;
constexpr int   ALMANAC_MAX_CARDS_PER_ROW = 14;
constexpr int   ALMANAC_CARD_H_SPACING = 1;
constexpr int   ALMANAC_CARD_V_SPACING = 4;

constexpr float PREVIEW_PLANT_X = 900.0f;
constexpr float PREVIEW_PLANT_Y = 198.0f;

void PlantAlmanacScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	AddTexture("IMAGE_ALMANAC_PLANTBACK", -90.0f, -20.0f, 1.0f, 1.0f, -1200, false);
	AddTexture("IMAGE_ALMANAC_PLANTCARD", 745.0f, 110.0f, 1.0f, 1.0f, -1000, false);
	AddTexture("IMAGE_ALMANAC_GROUNDDAY", 808.0f, 128.0f, 1.0f, 1.0f, -1100, false);

	mBackMenuButton = mUIManager.CreateButton(Vector(7, 560), Vector(162, 26));
	mBackMenuButton->SetAsCheckbox(false);
	mBackMenuButton->SetImageKeys(
		"IMAGE_ALMANAC_INDEXBUTTON",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT",
		"IMAGE_ALMANAC_INDEXBUTTONHIGHLIGHT");

	mBackMenuButton->SetText(u8"返回索引", ResourceKeys::Fonts::FONT_FZJZ, 18);
	mBackMenuButton->SetTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetHoverTextColor(glm::vec4(52, 51, 93, 255));
	mBackMenuButton->SetClickCallBack([this](bool) {
		this->mReadyToSwitchAlmanacScene = true;
		});

	RegisterDrawCommand("PlantInfo",
		[this](Graphics* g) {
			auto& app = GameAPP::GetInstance();
			if (!mCurrentPlantName.empty())
				app.DrawText(mCurrentPlantName, Vector(mPlantNameX, 290),
					glm::vec4(221, 157, 42, 255), ResourceKeys::Fonts::FONT_FZJZ, 24);

			float y = 337.0f;
			constexpr float LINE_HEIGHT = 22.0f;
			for (size_t i = 0; i < mDescriptionLines.size(); i++) {
				float x = (i == 0) ? 784.0f : 774.0f;
				app.DrawText(mDescriptionLines[i], Vector(x, y),
					glm::vec4(52, 51, 93, 255), ResourceKeys::Fonts::FONT_FZJZ, 17);
				y += LINE_HEIGHT;
			}
		},
		LAYER_UI + 100);

	SortDrawCommands();
}

void PlantAlmanacScene::CreateAllCards()
{
	const auto& haveCards = GameAPP::GetInstance().mHaveCards;
	auto& gameMgr = GameDataManager::GetInstance();

	int cardCount = 0;
	for (const auto& plantType : haveCards) {
		int row = cardCount / ALMANAC_MAX_CARDS_PER_ROW;
		int col = cardCount % ALMANAC_MAX_CARDS_PER_ROW;

		float posX = CARD_INIT_POSITION_X + col * (CARD_WIDTH + ALMANAC_CARD_H_SPACING);
		float posY = CARD_INIT_POSITION_Y + row * (CARD_HEIGHT + ALMANAC_CARD_V_SPACING);

		auto card = GameObjectManager::GetInstance().CreateGameObjectImmediate<Card>(
			LAYER_UI, plantType,
			gameMgr.GetPlantSunCost(plantType),
			gameMgr.GetPlantCooldown(plantType),
			true);

		if (auto transform = card->GetComponent<TransformComponent>()) {
			transform->SetPosition(Vector(posX, posY));
		}
		card->SetOriginalPosition(Vector(posX, posY));
		card->mIsUI = true;

		if (auto clickable = card->GetComponent<ClickableComponent>()) {
			clickable->onClick = [this, plantType]() {
				OnCardClicked(plantType);
				};
		}

		mCards.push_back(card);
		cardCount++;
	}
}

void PlantAlmanacScene::OnCardClicked(PlantType type)
{
	DestroyPreviewPlant();
	CreatePreviewPlant(type);
	UpdatePlantInfo(type);
}

void PlantAlmanacScene::CreatePreviewPlant(PlantType type)
{
	auto plant = GameAPP::GetInstance().InstantiatePlant(type, nullptr, -1, -1, true);
	if (!plant) return;
	mPreviewPlant = plant;
	plant->SetPosition(Vector(PREVIEW_PLANT_X, PREVIEW_PLANT_Y));
}

void PlantAlmanacScene::DestroyPreviewPlant()
{
	if (auto plant = mPreviewPlant.lock()) {
		GameObjectManager::GetInstance().DestroyGameObject(plant);
		mPreviewPlant.reset();
	}
}

void PlantAlmanacScene::Update()
{
	Scene::Update();

	if (mReadyToSwitchAlmanacScene) {
		mReadyToSwitchAlmanacScene = false;
		SceneManager::GetInstance().SwitchTo("AlmanacScene");
	}
}

void PlantAlmanacScene::OnEnter()
{
	Scene::OnEnter();
	LoadInfoFile();
	CreateAllCards();

	const auto& haveCards = GameAPP::GetInstance().mHaveCards;
	if (!haveCards.empty()) {
		CreatePreviewPlant(haveCards[0]);
		UpdatePlantInfo(haveCards[0]);
	}
}

void PlantAlmanacScene::OnExit()
{
	mCards.clear();
	mPreviewPlant.reset();
	mBackMenuButton.reset();
	mInfoMap.clear();
	mCurrentPlantName.clear();
	mDescriptionLines.clear();
	mCurrentPlantType = PlantType::NUM_PLANT_TYPES;
	Scene::OnExit();
}

void PlantAlmanacScene::LoadInfoFile()
{
	mInfoMap.clear();
	std::ifstream file("./resources/info.txt");
	if (!file.is_open()) return;

	std::string currentKey;
	std::string currentValue;

	auto trimRight = [](std::string& s) {
		while (!s.empty() && (s.back() == ' ' || s.back() == '\t' ||
			s.back() == '\r' || s.back() == '\n'))
			s.pop_back();
	};

	std::string line;
	while (std::getline(file, line)) {
		if (!line.empty() && line.back() == '\r')
			line.pop_back();

		if (line.size() >= 2 && line.front() == '[' && line.back() == ']') {
			if (!currentKey.empty()) {
				trimRight(currentValue);
				mInfoMap[currentKey] = currentValue;
			}
			currentKey = line.substr(1, line.size() - 2);
			currentValue.clear();
		}
		else if (!currentKey.empty()) {
			if (!currentValue.empty())
				currentValue += '\n';
			currentValue += line;
		}
	}
	if (!currentKey.empty()) {
		trimRight(currentValue);
		mInfoMap[currentKey] = currentValue;
	}
}

void PlantAlmanacScene::UpdatePlantInfo(PlantType type)
{
	mCurrentPlantType = type;
	std::string enumName = GameDataManager::GetInstance().PlantTypeToEnumName(type);

	auto nameIt = mInfoMap.find(enumName);
	mCurrentPlantName = (nameIt != mInfoMap.end()) ? nameIt->second : "";

	mPlantNameX = 899.5f;
	if (!mCurrentPlantName.empty()) {
		TTF_Font* font = ResourceManager::GetInstance().GetFont(
			ResourceKeys::Fonts::FONT_FZJZ, 24);
		if (font) {
			int tw = 0, th = 0;
			TTF_SizeUTF8(font, mCurrentPlantName.c_str(), &tw, &th);
			mPlantNameX = (761.0f + 1038.0f - tw) / 2.0f;
		}
	}

	auto descIt = mInfoMap.find(enumName + "_DESCRIPTION");
	std::string description = (descIt != mInfoMap.end()) ? descIt->second : "";

	mDescriptionLines = WrapText(description, 784.0f, 1024.0f, 774.0f,
		ResourceKeys::Fonts::FONT_FZJZ, 17);
}

std::vector<std::string> PlantAlmanacScene::WrapText(const std::string& text,
	float startX, float maxX, float wrapX,
	const std::string& fontKey, int fontSize)
{
	std::vector<std::string> lines;
	if (text.empty()) return lines;

	TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);

	std::string currentLine;
	float lineStartX = startX;

	size_t i = 0;
	while (i < text.size()) {
		unsigned char c = static_cast<unsigned char>(text[i]);
		size_t charLen = 1;
		if ((c & 0x80) == 0)      charLen = 1;
		else if ((c & 0xE0) == 0xC0) charLen = 2;
		else if ((c & 0xF0) == 0xE0) charLen = 3;
		else if ((c & 0xF8) == 0xF0) charLen = 4;

		if (i + charLen > text.size()) break;
		std::string ch = text.substr(i, charLen);
		std::string testLine = currentLine + ch;

		int lineWidth = 0, lineHeight = 0;
		if (font)
			TTF_SizeUTF8(font, testLine.c_str(), &lineWidth, &lineHeight);
		else
			lineWidth = static_cast<int>(testLine.size()) * (fontSize / 2);

		if (lineStartX + lineWidth > maxX && !currentLine.empty()) {
			lines.push_back(currentLine);
			currentLine.clear();
			lineStartX = wrapX;
			continue;
		}

		currentLine += ch;
		i += charLen;
	}
	if (!currentLine.empty())
		lines.push_back(currentLine);

	return lines;
}
