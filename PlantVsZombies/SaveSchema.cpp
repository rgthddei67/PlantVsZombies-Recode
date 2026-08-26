#include "SaveSchema.h"
#include "Game/Plant/PlantType.h"

#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace {
	enum class DocumentKind {
		Player,
		Level,
	};

	bool ReadSchemaVersion(const nlohmann::json& document, int currentVersion,
		const char* documentName, int& version, std::string& error)
	{
		if (!document.is_object()) {
			error = std::string(documentName) + "存档根节点必须是对象";
			return false;
		}
		if (!document.contains("schemaVersion")) {
			version = 0;
			return true;
		}

		const auto& value = document["schemaVersion"];
		if (value.is_number_unsigned()) {
			const std::uint64_t raw = value.get<std::uint64_t>();
			if (raw > static_cast<std::uint64_t>(currentVersion)) {
				error = std::string(documentName) + "存档来自更高版本";
				return false;
			}
			version = static_cast<int>(raw);
			return true;
		}
		if (!value.is_number_integer()) {
			error = std::string(documentName) + "存档 schemaVersion 必须是整数";
			return false;
		}

		const std::int64_t raw = value.get<std::int64_t>();
		if (raw < 0) {
			error = std::string(documentName) + "存档 schemaVersion 不能为负数";
			return false;
		}
		if (raw > currentVersion) {
			error = std::string(documentName) + "存档来自更高版本";
			return false;
		}
		version = static_cast<int>(raw);
		return true;
	}

	/** 为已经通关 3-8 的旧玩家补发前移后的毒囊射手，且不重复已有卡片。 */
	void MigrateMovedToxicPeaReward(nlohmann::json& document)
	{
		constexpr int kFirstLevelAfterReward = 27; // 3-8 通关后推进到的内部冒险关卡
		if (!document.contains("adventureLevel") ||
			!document["adventureLevel"].is_number_integer() ||
			document["adventureLevel"].get<int>() < kFirstLevelAfterReward ||
			!document.contains("havecards") || !document["havecards"].is_array()) {
			return;
		}

		auto& cards = document["havecards"];
		const int reward = static_cast<int>(PlantType::PLANT_TOXICPEASHOOTER);
		const bool alreadyOwned = std::any_of(cards.begin(), cards.end(),
			[](const nlohmann::json& card) {
				return card.is_number_integer() && card.get<int>() ==
					static_cast<int>(PlantType::PLANT_TOXICPEASHOOTER);
			});
		if (!alreadyOwned) cards.push_back(reward);
	}

	/** 在副本上按文档类型执行迁移，全部成功后才提交，避免失败留下半迁移文档。 */
	bool UpgradeDocument(nlohmann::json& document, int currentVersion,
		const char* documentName, DocumentKind kind, std::string& error)
	{
		error.clear();
		int version = 0;
		if (!ReadSchemaVersion(document, currentVersion,
			documentName, version, error)) {
			return false;
		}

		nlohmann::json upgraded = document;
		while (version < currentVersion) {
			switch (version) {
			case 0:
				// v1 只建立显式版本入口；历史字段与缺省兼容规则保持不变。
				version = 1;
				upgraded["schemaVersion"] = version;
				break;
			case 1:
				if (kind == DocumentKind::Player) {
					// 玩家 v2 对已通关 3-8 的旧档补发前移后的植物奖励。
					MigrateMovedToxicPeaReward(upgraded);
				}
				// 关卡 v2 加入四大关雾势字段；旧档由关卡上下文重建，迁移层不伪造状态。
				version = 2;
				upgraded["schemaVersion"] = version;
				break;
			case 2: {
				if (kind == DocumentKind::Player) {
					// 玩家 v3 新增高级暂停设置；旧玩家默认进入更严格的普通暂停。
					if (!upgraded.contains("advancedPauseEnabled")) {
						upgraded["advancedPauseEnabled"] = false;
					}
				}
				else {
					// 关卡 v3 把旧 CLEAR/DENSE 二态扩成 DEFAULT/SMALL/NORMAL/DENSE，保留旧档视觉强度。
					constexpr const char* kFogIntensityKeys[] = {
						"fogWeatherIntensity",
						"forecastFogWeatherIntensity",
						"actualForecastFogWeatherIntensity",
					};
					for (const char* key : kFogIntensityKeys) {
						if (!upgraded.contains(key) || !upgraded[key].is_number_integer()) continue;
						const int oldValue = upgraded[key].get<int>();
						if (oldValue == 0) upgraded[key] = 1;      // 旧 CLEAR 是双层并扩 1 格，对应小雾
						else if (oldValue == 1) upgraded[key] = 3; // 旧 DENSE 仍是大雾
					}
				}
				version = 3;
				upgraded["schemaVersion"] = version;
				break;
			}
			case 3:
				if (kind == DocumentKind::Player) {
					if (!upgraded.contains("lastSelectedCards")) {
						// 玩家 v4 新增上次已提交的选卡；旧档从空记录开始，不猜测历史关卡卡组。
						upgraded["lastSelectedCards"] = nlohmann::json::array();
					}
				}
				else {
					// 关卡 v4 新增冰裂钻机每波预算和独立地裂；旧档没有已提交威胁，从单位元恢复。
					if (!upgraded.contains("iceCrackDrillsSpawnedThisWave")) {
						upgraded["iceCrackDrillsSpawnedThisWave"] = 0;
					}
					if (!upgraded.contains("groundRifts")) {
						upgraded["groundRifts"] = nlohmann::json::array();
					}
				}
				version = 4;
				upgraded["schemaVersion"] = version;
				break;
			case 4:
				if (kind == DocumentKind::Player) {
					if (!upgraded.contains("crazyDaveTutorialsSeen")) {
						// 玩家 v5 新增按冒险关卡号保存的戴夫闲聊已读集合；旧档从未读开始。
						upgraded["crazyDaveTutorialsSeen"] = nlohmann::json::array();
					}
				}
				else {
					// 关卡 v5 保存整栏天气干扰结果与气象干扰僵尸每波名额；旧档均从单位元恢复。
					if (!upgraded.contains("weatherForecastDisrupted")) {
						upgraded["weatherForecastDisrupted"] = false;
					}
					if (!upgraded.contains("fogWeatherForecastDisrupted")) {
						upgraded["fogWeatherForecastDisrupted"] = false;
					}
					if (!upgraded.contains("weatherJammersSpawnedThisWave")) {
						upgraded["weatherJammersSpawnedThisWave"] = 0;
					}
				}
				version = 5;
				upgraded["schemaVersion"] = version;
				break;
			case 5:
				if (kind == DocumentKind::Level
					&& !upgraded.contains("redeyeGargantuarsSpawnedThisWave")) {
					// 关卡 v6 增加冒险红眼每波上限计数；旧档从尚未消费的单位元恢复。
					upgraded["redeyeGargantuarsSpawnedThisWave"] = 0;
				}
				version = 6;
				upgraded["schemaVersion"] = version;
				break;
			default:
				error = std::string(documentName) + "存档缺少迁移路径";
				return false;
			}
		}
		document = std::move(upgraded);
		return true;
	}
}

bool SaveSchema::UpgradePlayerDocument(
	nlohmann::json& document, std::string& error)
{
	return UpgradeDocument(document, kCurrentPlayerVersion,
		"玩家", DocumentKind::Player, error);
}

bool SaveSchema::UpgradeLevelDocument(
	nlohmann::json& document, std::string& error)
{
	return UpgradeDocument(document, kCurrentLevelVersion,
		"关卡", DocumentKind::Level, error);
}
