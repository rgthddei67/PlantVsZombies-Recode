#pragma once
#ifndef _GAMEINFOSAVER_H
#define _GAMEINFOSAVER_H
#include "FileManager.h"
#include <string>

class Board;
class CardSlotManager;

class GameInfoSaver {
public:
	bool SavePlayerInfo();
	bool LoadPlayerInfo();

	bool SaveLevelData(Board* board, CardSlotManager* manager);
	bool LoadLevelData(Board* board, CardSlotManager* manager);
	bool DeleteLevelData(Board* board);

	/**
	 * @brief 将当前关卡用正式序列化逻辑写入显式 AutoTest 快照路径。
	 * @details 仅在 AutoTest 模式可用，不访问玩家存档根目录。
	 */
	bool SaveAutoTestLevelSnapshot(Board* board, CardSlotManager* manager,
		const std::string& filename);
	/**
	 * @brief 为下一次 GameScene 正常加载阶段登记一次性 AutoTest 快照路径。
	 * @details 路径在加载尝试开始前即清除，成功或失败都不会影响后续普通 goto_level。
	 */
	bool QueueAutoTestLevelSnapshotLoad(const std::string& filename);
	/** 取得并清除最近一次快照加载结果；失败原因写入 error。 */
	bool ConsumeAutoTestLevelSnapshotLoadResult(std::string& error);
	void CancelAutoTestLevelSnapshotLoad();

private:
	// 实际序列化逻辑（须为成员：Board 仅 friend 本类，自由函数无私有成员访问权）。
	// 上面的公有接口只做一层 try/catch 包裹，详见 .cpp 的“异常安全边界”。
	static bool SavePlayerInfoImpl();
	static bool LoadPlayerInfoImpl();
	static bool SaveLevelDataImpl(Board* board, CardSlotManager* manager);
	bool LoadLevelDataImpl(Board* board, CardSlotManager* manager);
	/** 构建完整正式关卡 JSON，并写入调用方指定的已隔离路径。 */
	static bool SerializeLevelDataToPath(Board* board, CardSlotManager* manager,
		const std::string& filename);
	/** 从指定路径解析 JSON，并用正式反序列化流程应用到新 Board。 */
	static bool DeserializeLevelDataFromPath(Board* board, CardSlotManager* manager,
		const std::string& filename);

	std::string mAutoTestSnapshotLoadPath;
	bool mAutoTestSnapshotLoadAttempted = false;
	bool mAutoTestSnapshotLoadSucceeded = false;
	std::string mAutoTestSnapshotLoadError;
};

#endif
