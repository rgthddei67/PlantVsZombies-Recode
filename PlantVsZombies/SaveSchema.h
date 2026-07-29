#pragma once

#include <nlohmann/json_fwd.hpp>
#include <string>

namespace SaveSchema {
	inline constexpr int kCurrentPlayerVersion = 1;
	inline constexpr int kCurrentLevelVersion = 2;

	/**
	 * 将玩家配置文档事务式升级到当前结构。
	 * 旧档缺少 schemaVersion 时视为版本 0；未来版本或损坏结构会被拒绝且不修改输入。
	 */
	bool UpgradePlayerDocument(nlohmann::json& document, std::string& error);

	/**
	 * 将关卡快照事务式升级到当前结构。
	 * 迁移只补结构元数据，不改变旧字段的玩法语义。
	 */
	bool UpgradeLevelDocument(nlohmann::json& document, std::string& error);
}
