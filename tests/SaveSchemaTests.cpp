#include "SaveSchema.h"

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
	TestCurrentLevelDocumentIsStable();
	TestLegacyLevelUpgradePreservesGameplayState();
	TestFutureVersionIsRejectedTransactionally();
	TestInvalidRootAndVersionAreRejected();

	if (gFailureCount == 0) {
		std::cout << "SaveSchemaTests passed\n";
	}
	return gFailureCount == 0 ? 0 : 1;
}
