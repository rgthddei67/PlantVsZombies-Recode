#include "ZombieAlmanacScene.h"
#include "SceneManager.h"
#include "../GameApp.h"
#include "AdventureProgression.h"
#include "Zombie/Zombie.h"
#include "Zombie/BobsledTeamZombie.h"
#include "ClickableComponent.h"
#include "ShadowComponent.h"
#include "./Plant/GameDataManager.h"
#include "GameObjectManager.h"
#include <sstream>
#include <algorithm>
#include <array>
#include "../FileManager.h"

constexpr float ZOMBIE_GRID_INIT_X = 70.0f;
constexpr float ZOMBIE_GRID_INIT_Y = 120.0f;
constexpr int   ZOMBIE_WINDOW_SIZE = 76;
constexpr int   ZOMBIE_MAX_PER_ROW = 8;
constexpr int   ZOMBIE_H_SPACING = 4;
constexpr int   ZOMBIE_V_SPACING = 4;

constexpr float PREVIEW_ZOMBIE_X = 900.0f;
constexpr float PREVIEW_ZOMBIE_Y = 280.0f;
constexpr float PREVIEW_BOBSLED_LEADER_X = 850.0f; // 四人雪橇详情预览左移，给 150px 编队跨度留出卡片空间

// 描述书写区（羊皮纸内沿，屏幕坐标）。卡片图 IMAGE_ALMANAC_ZOMBIECARD 绘于 (745,110)，
// 324x497；僵尸预览窗更大，书写区比植物卡更靠下、更矮。
constexpr float DESC_START_X    = 784.0f;        // 第一行起点（首行略缩进）
constexpr float DESC_WRAP_X     = 774.0f;        // 后续行起点
constexpr float DESC_MAX_X      = 1024.0f;       // 折行右界
constexpr float DESC_START_Y    = 410.0f;        // 第一行顶部
constexpr float DESC_BOTTOM_Y   = 580.0f;        // 书写区下沿（羊皮纸底边内留余量）
constexpr int   DESC_FONT_MAX   = 17;            // 默认/最大字号
constexpr int   DESC_FONT_MIN   = 10;            // 收缩下限（再小不可读）
constexpr float DESC_LINE_RATIO = 22.0f / 17.0f; // 行高随字号等比（原 17 号配 22px）

void ZombieAlmanacScene::BuildDrawCommands()
{
	Scene::BuildDrawCommands();
	AddTexture("IMAGE_ALMANAC_ZOMBIEBACK", -90.0f, -20.0f, 1.0f, 1.0f, -1200, false);
	AddTexture("IMAGE_ALMANAC_ZOMBIECARD", 745.0f, 110.0f, 1.0f, 1.0f, -1000, false);
	AddTexture("IMAGE_ALMANAC_GROUNDDAY", 808.0f, 150.0f, 1.0f, 1.0f, -1100, false);

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

	RegisterDrawCommand("ZombieWindowBack",
		[this](Graphics* g) {
			auto& resMgr = ResourceManager::GetInstance();
			const Texture* windowTex = resMgr.GetTexture("IMAGE_ALMANAC_ZOMBIEWINDOW");
			for (const auto& pos : mGridPositions) {
				if (windowTex)
					g->DrawTexture(windowTex, pos.x, pos.y,
						static_cast<float>(windowTex->width),
						static_cast<float>(windowTex->height));
			}
		},
		LAYER_GAME_OBJECT - 1);

	RegisterDrawCommand("ZombieWindowFront",
		[this](Graphics* g) {
			auto& resMgr = ResourceManager::GetInstance();
			const Texture* window2Tex = resMgr.GetTexture("IMAGE_ALMANAC_ZOMBIEWINDOW2");
			for (const auto& pos : mGridPositions) {
				if (window2Tex)
					g->DrawTexture(window2Tex, pos.x, pos.y,
						static_cast<float>(window2Tex->width),
						static_cast<float>(window2Tex->height));
			}
		},
		LAYER_GAME_OBJECT + 1);

	RegisterDrawCommand("ZombieInfo",
		[this](Graphics* g) {
			auto& app = GameAPP::GetInstance();
			if (!mCurrentZombieName.empty())
				app.DrawText(mCurrentZombieName, Vector(mZombieNameX, 368.0f),
					glm::vec4(221, 157, 42, 255), ResourceKeys::Fonts::FONT_FZJZ, 24);

			float y = DESC_START_Y;
			for (size_t i = 0; i < mDescriptionLines.size(); i++) {
				float x = (i == 0) ? DESC_START_X : DESC_WRAP_X;
				app.DrawText(mDescriptionLines[i], Vector(x, y),
					glm::vec4(52, 51, 93, 255),
					ResourceKeys::Fonts::FONT_FZJZ, mDescriptionFontSize);
				y += mDescriptionLineHeight;
			}
		},
		LAYER_UI + 100);

	SortDrawCommands();
}

std::vector<ZombieType> ZombieAlmanacScene::LoadEncounteredZombieTypes() const
{
	std::vector<ZombieType> encounteredTypes;
	auto& gameApp = GameAPP::GetInstance();
	const int completedThrough = std::min(
		gameApp.mAdventureLevel - 1,
		AdventureProgression::LAST_ADVENTURE_LEVEL);

	auto& gameData = GameDataManager::GetInstance();
	std::array<bool, static_cast<std::size_t>(ZombieType::NUM_ZOMBIE_TYPES)> seen{};
	auto appendType = [&](ZombieType type) {
		const auto index = static_cast<std::size_t>(type);
		if (index >= seen.size() || seen[index] || !gameData.HasZombie(type)) return;
		seen[index] = true;
		encounteredTypes.push_back(type);
	};

	nlohmann::json spawnLists;
	if (completedThrough >= 1
		&& FileManager::LoadJsonFile("./resources/spawnlists.json", spawnLists)
		&& spawnLists.is_array()) {
		// 当前关不计入：玩家只有通关后才必然见过该关池中的僵尸。
		// 按关卡递增而不是依赖 JSON 文件顺序，使网格始终保持首次遭遇顺序。
		for (int completedLevel = 1; completedLevel <= completedThrough; ++completedLevel) {
			for (const auto& entry : spawnLists) {
				if (!entry.is_object()) continue;
				auto levelIt = entry.find("level");
				if (levelIt == entry.end() || !levelIt->is_number_integer()
					|| levelIt->get<int>() != completedLevel) {
					continue;
				}

				auto zombiesIt = entry.find("zombies");
				if (zombiesIt == entry.end() || !zombiesIt->is_array()) break;
				for (const auto& value : *zombiesIt) {
					if (!value.is_number_integer()) continue;
					const int rawType = value.get<int>();
					if (rawType < 0
						|| rawType >= static_cast<int>(ZombieType::NUM_ZOMBIE_TYPES)) {
						continue;
					}

					const ZombieType type = static_cast<ZombieType>(rawType);
					appendType(type);

					// 伴舞不会直接进入随机池，但玩家遇到舞王时必然会见到它。
					if (type == ZombieType::ZOMBIE_DANCER
						|| type == ZombieType::ZOMBIE_ELITE_DANCER) {
						appendType(ZombieType::ZOMBIE_BACKUP_DANCER);
					}
				}
				break;
			}
		}
	}

	// 精英舞王是概率天气变异，不能由 spawnlist 推断；只合并实际出生后写入的永久记录。
	if (gameApp.HasEncounteredEliteDancer()) {
		appendType(ZombieType::ZOMBIE_ELITE_DANCER);
	}

	return encounteredTypes;
}

void ZombieAlmanacScene::CreateAllZombieEntries()
{
	mDisplayedZombieTypes = LoadEncounteredZombieTypes();

	int entryCount = 0;
	for (const auto& zombieType : mDisplayedZombieTypes) {
		int row = entryCount / ZOMBIE_MAX_PER_ROW;
		int col = entryCount % ZOMBIE_MAX_PER_ROW;

		float frameX = ZOMBIE_GRID_INIT_X + col * (ZOMBIE_WINDOW_SIZE + ZOMBIE_H_SPACING);
		float frameY = ZOMBIE_GRID_INIT_Y + row * (ZOMBIE_WINDOW_SIZE + ZOMBIE_V_SPACING);
		mGridPositions.push_back(Vector(frameX, frameY));

		Vector offset = GameDataManager::GetInstance().GetZombieOffset(zombieType);
		float zombieX = frameX + ZOMBIE_WINDOW_SIZE / 2.0f - offset.x - 18.0f;
		float zombieY = frameY + ZOMBIE_WINDOW_SIZE / 2.0f - offset.y - 30.0f;

		auto zombie = GameAPP::GetInstance().InstantiateZombieFree(
			zombieType, nullptr, zombieX, zombieY);
		if (!zombie) {
			entryCount++;
			continue;
		}

		zombie->RemoveShadow();
		zombie->PauseAnimation();
		zombie->mIsUI = true;
		zombie->SetRenderOrder(LAYER_UI + 51);
		if (auto transform = zombie->GetTransform()) {
			float s = 0.7f;
			transform->SetScale(s);
		}

		auto clickable = zombie->CreateClickable();
		clickable->SetClickArea(Vector(ZOMBIE_WINDOW_SIZE, ZOMBIE_WINDOW_SIZE));
		clickable->SetClickOffset(Vector(
			offset.x - ZOMBIE_WINDOW_SIZE / 2.0f,
			offset.y - ZOMBIE_WINDOW_SIZE / 2.0f));
		clickable->onClick = [this, zombieType]() {
			OnZombieClicked(zombieType);
			};

		mGridZombies.push_back(zombie);
		constexpr int CLIP_INSET = 3;
		zombie->SetClipRect(
			static_cast<int>(frameX) + CLIP_INSET,
			static_cast<int>(frameY) + CLIP_INSET,
			ZOMBIE_WINDOW_SIZE - 2 * CLIP_INSET,
			ZOMBIE_WINDOW_SIZE - 2 * CLIP_INSET);
		entryCount++;
	}
}

void ZombieAlmanacScene::OnZombieClicked(ZombieType type)
{
	DestroyPreviewZombie();
	CreatePreviewZombie(type);
	UpdateZombieInfo(type);
}

void ZombieAlmanacScene::CreatePreviewZombie(ZombieType type)
{
	const float previewX = type == ZombieType::ZOMBIE_BOBSLED_TEAM
		? PREVIEW_BOBSLED_LEADER_X : PREVIEW_ZOMBIE_X;
	auto zombie = GameAPP::GetInstance().InstantiateZombieFree(
		type, nullptr, previewX, PREVIEW_ZOMBIE_Y);
	if (!zombie) return;
	mPreviewZombie = zombie;
	mPreviewZombieMembers.push_back(zombie);

	if (auto* leader = dynamic_cast<BobsledTeamZombie*>(zombie.get())) {
		leader->ConfigurePreviewTeamMember(0);
		for (int slot = 1; slot < 4; ++slot) {
			auto member = GameAPP::GetInstance().InstantiateZombieFree(
				type, nullptr,
				previewX + BobsledTeamZombie::GetPreviewMemberOffsetX(slot),
				PREVIEW_ZOMBIE_Y);
			if (member) mPreviewZombieMembers.push_back(member);
			if (auto* rider = dynamic_cast<BobsledTeamZombie*>(member.get())) {
				rider->ConfigurePreviewTeamMember(slot);
			}
		}
	}
}

void ZombieAlmanacScene::DestroyPreviewZombie()
{
	for (const auto& weakMember : mPreviewZombieMembers) {
		if (auto member = weakMember.lock()) {
			GameObjectManager::GetInstance().DestroyGameObject(member);
		}
	}
	mPreviewZombieMembers.clear();
	mPreviewZombie.reset();
}

void ZombieAlmanacScene::Update()
{
	Scene::Update();

	if (mReadyToSwitchAlmanacScene) {
		mReadyToSwitchAlmanacScene = false;
		SceneManager::GetInstance().SwitchTo("AlmanacScene");
	}
}

void ZombieAlmanacScene::OnEnter()
{
	Scene::OnEnter();
	LoadInfoFile();
	CreateAllZombieEntries();

	if (!mDisplayedZombieTypes.empty()) {
		CreatePreviewZombie(mDisplayedZombieTypes.front());
		UpdateZombieInfo(mDisplayedZombieTypes.front());
	}
}

void ZombieAlmanacScene::OnExit()
{
	mGridZombies.clear();
	mGridPositions.clear();
	mDisplayedZombieTypes.clear();
	mPreviewZombie.reset();
	mPreviewZombieMembers.clear();
	mBackMenuButton.reset();
	mInfoMap.clear();
	mCurrentZombieName.clear();
	mDescriptionLines.clear();
	mCurrentZombieType = ZombieType::NUM_ZOMBIE_TYPES;
	Scene::OnExit();
}

void ZombieAlmanacScene::LoadInfoFile()
{
	mInfoMap.clear();
	std::string content = FileManager::LoadFileAsString("./resources/info.txt");
	if (content.empty()) return;
	std::istringstream file(content);

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

void ZombieAlmanacScene::UpdateZombieInfo(ZombieType type)
{
	mCurrentZombieType = type;
	std::string enumName = GameDataManager::GetInstance().ZombieTypeToEnumName(type);

	auto nameIt = mInfoMap.find(enumName);
	mCurrentZombieName = (nameIt != mInfoMap.end()) ? nameIt->second : "";

	mZombieNameX = 899.5f;
	if (!mCurrentZombieName.empty()) {
		TTF_Font* font = ResourceManager::GetInstance().GetFont(
			ResourceKeys::Fonts::FONT_FZJZ, 24);
		if (font) {
			int tw = 0, th = 0;
			TTF_SizeUTF8(font, mCurrentZombieName.c_str(), &tw, &th);
			mZombieNameX = (761.0f + 1038.0f - tw) / 2.0f;
		}
	}

	auto descIt = mInfoMap.find(enumName + "_DESCRIPTION");
	std::string description = (descIt != mInfoMap.end()) ? descIt->second : "";

	// 按书写区高度自动收缩字号：从最大字号往下试，直到整段折行后能装进 [START_Y, BOTTOM_Y]。
	// 字号变小 → 行高变小 + 每行容字更多 → 总高单调下降，线性试探即可收敛。最小字号兜底。
	mDescriptionLines.clear();
	mDescriptionFontSize = DESC_FONT_MIN;
	mDescriptionLineHeight = DESC_FONT_MIN * DESC_LINE_RATIO;
	for (int fs = DESC_FONT_MAX; fs >= DESC_FONT_MIN; --fs) {
		auto lines = WrapText(description, DESC_START_X, DESC_MAX_X, DESC_WRAP_X,
			ResourceKeys::Fonts::FONT_FZJZ, fs);
		float lineHeight = fs * DESC_LINE_RATIO;
		mDescriptionLines = std::move(lines);
		mDescriptionFontSize = fs;
		mDescriptionLineHeight = lineHeight;
		if (DESC_START_Y + mDescriptionLines.size() * lineHeight <= DESC_BOTTOM_Y)
			break;
	}
}

std::vector<std::string> ZombieAlmanacScene::WrapText(const std::string& text,
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
