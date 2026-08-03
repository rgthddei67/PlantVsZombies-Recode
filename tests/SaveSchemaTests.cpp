#include "SaveSchema.h"
#include "Game/Plant/PlantType.h"

#include <algorithm>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

namespace {
	int gFailureCount = 0;

	void Expect(bool condition, const char* message) {
		if (!condition) {
			std::cerr << "FAILED: " << message << '\n';
			++gFailureCount;
		}
	}

	void TestLegacyPlayerUpgradePreservesFields() {
		nlohmann::json document = {
			{ "difficulty", 3 },
			{ "havecards", { 1, 2, 5 } },
			{ "nested", { { "kept", true } } }
		};
		const auto legacy = document;
		std::string error;

		Expect(SaveSchema::UpgradePlayerDocument(document, error),
			"无版本玩家旧档应升级成功");
		Expect(error.empty(), "成功迁移不应返回错误");
		Expect(document["schemaVersion"] == SaveSchema::kCurrentPlayerVersion,
			"玩家旧档应写入当前版本");
		for (const auto& [key, value] : legacy.items()) {
			Expect(document[key] == value, "玩家旧字段必须原样保留");
		}
	}

	void TestMovedToxicRewardPlayerUpgrade() {
		const int toxicPea = static_cast<int>(PlantType::PLANT_TOXICPEASHOOTER);
		auto countToxicPea = [toxicPea](const nlohmann::json& document) {
			return static_cast<int>(std::count(
				document["havecards"].begin(), document["havecards"].end(), toxicPea));
		};
		std::string error;

		nlohmann::json beforeCompletion = {
			{ "schemaVersion", 1 },
			{ "adventureLevel", 26 },
			{ "havecards", { static_cast<int>(PlantType::PLANT_PEASHOOTER) } }
		};
		Expect(SaveSchema::UpgradePlayerDocument(beforeCompletion, error),
			"尚未通关3-8的玩家档应升级成功");
		Expect(countToxicPea(beforeCompletion) == 0,
			"尚未通关3-8时不得提前补发毒囊射手");

		nlohmann::json afterCompletion = {
			{ "schemaVersion", 1 },
			{ "adventureLevel", 27 },
			{ "havecards", { static_cast<int>(PlantType::PLANT_PEASHOOTER) } }
		};
		Expect(SaveSchema::UpgradePlayerDocument(afterCompletion, error),
			"已通关3-8的玩家档应升级成功");
		Expect(afterCompletion["schemaVersion"] == SaveSchema::kCurrentPlayerVersion,
			"奖励迁移后应写入当前玩家版本");
		Expect(countToxicPea(afterCompletion) == 1,
			"已通关3-8的旧档应补发一次毒囊射手");

		nlohmann::json alreadyOwned = {
			{ "schemaVersion", 1 },
			{ "adventureLevel", 36 },
			{ "havecards", {
				static_cast<int>(PlantType::PLANT_PEASHOOTER), toxicPea } }
		};
		Expect(SaveSchema::UpgradePlayerDocument(alreadyOwned, error),
			"已拥有毒囊射手的玩家档应升级成功");
		Expect(countToxicPea(alreadyOwned) == 1,
			"玩家档迁移不得重复已有毒囊射手");
	}

	void TestCurrentPlayerDocumentIsStable() {
		nlohmann::json document = {
			{ "schemaVersion", SaveSchema::kCurrentPlayerVersion },
			{ "adventureLevel", 27 },
			{ "havecards", { static_cast<int>(PlantType::PLANT_PEASHOOTER) } }
		};
		const auto before = document;
		std::string error;

		Expect(SaveSchema::UpgradePlayerDocument(document, error),
			"当前版本玩家档应通过");
		Expect(document == before, "当前版本玩家档不应重复执行历史迁移");
	}

	void TestCurrentLevelDocumentIsStable() {
		nlohmann::json document = {
			{ "schemaVersion", SaveSchema::kCurrentLevelVersion },
			{ "currentWave", 8 },
			{ "weatherTimer", 12.5f }
		};
		const auto before = document;
		std::string error;

		Expect(SaveSchema::UpgradeLevelDocument(document, error),
			"当前版本关卡档应通过");
		Expect(document == before, "当前版本关卡档不应被改写");
	}

	void TestLegacyLevelUpgradePreservesGameplayState() {
		nlohmann::json document = {
			{ "boardState", 1 },
			{ "currentWave", 20 },
			{ "weatherForecastReady", true },
			{ "zombies", nlohmann::json::array({ { { "id", 7 } } }) }
		};
		const auto legacy = document;
		std::string error;

		Expect(SaveSchema::UpgradeLevelDocument(document, error),
			"无版本关卡旧档应升级成功");
		Expect(document["schemaVersion"] == SaveSchema::kCurrentLevelVersion,
			"关卡旧档应写入当前版本");
		for (const auto& [key, value] : legacy.items()) {
			Expect(document[key] == value, "关卡玩法字段必须原样保留");
		}
	}

	void TestVersionOneLevelUpgradeDefersFogInitializationToBoard() {
		nlohmann::json document = {
			{ "schemaVersion", 1 },
			{ "boardState", 1 },
			{ "weatherTimer", 12.5f }
		};
		const auto legacy = document;
		std::string error;

		Expect(SaveSchema::UpgradeLevelDocument(document, error),
			"v1 关卡档应升级到雾势结构版本");
		Expect(document["schemaVersion"] == SaveSchema::kCurrentLevelVersion,
			"v1 关卡档应写入当前版本");
		Expect(!document.contains("fogWeatherInitialized"),
			"迁移层不能在缺少关卡上下文时伪造雾势初始化状态");
		for (const auto& [key, value] : legacy.items()) {
			if (key == "schemaVersion") continue;
			Expect(document[key] == value, "v1 关卡玩法字段必须原样保留");
		}
	}

	void TestVersionTwoLevelUpgradePreservesFogStrength() {
		nlohmann::json document = {
			{ "schemaVersion", 2 },
			{ "fogWeatherInitialized", true },
			{ "fogWeatherIntensity", 0 },
			{ "forecastFogWeatherIntensity", 1 },
			{ "actualForecastFogWeatherIntensity", 0 },
			{ "fogWeatherForecastReady", true }
		};
		std::string error;

		Expect(SaveSchema::UpgradeLevelDocument(document, error),
			"v2 关卡档应升级到四档雾势结构");
		Expect(document["schemaVersion"] == SaveSchema::kCurrentLevelVersion,
			"v2 关卡档应写入当前版本");
		Expect(document["fogWeatherIntensity"] == 1,
			"旧 CLEAR 的双层扩展表现应迁移为小雾");
		Expect(document["forecastFogWeatherIntensity"] == 3,
			"旧 DENSE 预报应继续表示大雾");
		Expect(document["actualForecastFogWeatherIntensity"] == 1,
			"旧 CLEAR 真实预报应迁移为小雾");
		Expect(document["fogWeatherForecastReady"] == true,
			"雾势枚举迁移不得改变预报锁定状态");
	}

	void TestFutureVersionIsRejectedTransactionally() {
		nlohmann::json document = {
			{ "schemaVersion", SaveSchema::kCurrentLevelVersion + 1 },
			{ "currentWave", 99 }
		};
		const auto before = document;
		std::string error;

		Expect(!SaveSchema::UpgradeLevelDocument(document, error),
			"未来版本关卡档必须拒绝");
		Expect(!error.empty(), "拒绝未来版本时应说明原因");
		Expect(document == before, "迁移失败不得修改原文档");
	}

	void TestInvalidRootAndVersionAreRejected() {
		nlohmann::json arrayDocument = nlohmann::json::array({ 1, 2 });
		const auto beforeArray = arrayDocument;
		std::string error;
		Expect(!SaveSchema::UpgradePlayerDocument(arrayDocument, error),
			"非对象根节点必须拒绝");
		Expect(arrayDocument == beforeArray, "根节点校验失败不得修改输入");

		nlohmann::json invalidVersion = {
			{ "schemaVersion", "one" },
			{ "difficulty", 1 }
		};
		const auto beforeVersion = invalidVersion;
		Expect(!SaveSchema::UpgradePlayerDocument(invalidVersion, error),
			"非整数版本必须拒绝");
		Expect(invalidVersion == beforeVersion, "版本校验失败不得修改输入");

		nlohmann::json negativeVersion = {
			{ "schemaVersion", -1 },
			{ "difficulty", 1 }
		};
		const auto beforeNegative = negativeVersion;
		Expect(!SaveSchema::UpgradePlayerDocument(negativeVersion, error),
			"负版本号必须拒绝");
		Expect(negativeVersion == beforeNegative, "负版本校验失败不得修改输入");
	}
}

int main() {
	TestLegacyPlayerUpgradePreservesFields();
	TestMovedToxicRewardPlayerUpgrade();
	TestCurrentPlayerDocumentIsStable();
	TestCurrentLevelDocumentIsStable();
	TestLegacyLevelUpgradePreservesGameplayState();
	TestVersionOneLevelUpgradeDefersFogInitializationToBoard();
	TestVersionTwoLevelUpgradePreservesFogStrength();
	TestFutureVersionIsRejectedTransactionally();
	TestInvalidRootAndVersionAreRejected();

	if (gFailureCount == 0) {
		std::cout << "SaveSchemaTests passed\n";
	}
	return gFailureCount == 0 ? 0 : 1;
}
