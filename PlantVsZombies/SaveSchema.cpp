#include "SaveSchema.h"

#include <cstdint>
#include <nlohmann/json.hpp>
#include <string>
#include <utility>

namespace {
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

	/** 在副本上执行迁移，全部成功后才提交，避免失败路径留下半迁移文档。 */
	bool UpgradeDocument(nlohmann::json& document, int currentVersion,
		const char* documentName, std::string& error)
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
				// v2 加入四大关雾势字段；旧档必须由关卡上下文重建，迁移层不伪造初始化状态。
				version = 2;
				upgraded["schemaVersion"] = version;
				break;
			case 2: {
				// v3 把旧 CLEAR/DENSE 二态扩成 DEFAULT/SMALL/NORMAL/DENSE，保留旧档视觉强度。
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
				version = 3;
				upgraded["schemaVersion"] = version;
				break;
			}
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
	return UpgradeDocument(document, kCurrentPlayerVersion, "玩家", error);
}

bool SaveSchema::UpgradeLevelDocument(
	nlohmann::json& document, std::string& error)
{
	return UpgradeDocument(document, kCurrentLevelVersion, "关卡", error);
}
