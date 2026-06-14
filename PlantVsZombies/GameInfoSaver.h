#pragma once
#ifndef _GAMEINFOSAVER_H
#define _GAMEINFOSAVER_H
#include "FileManager.h"

class Board;
class CardSlotManager;

class GameInfoSaver {
public:
	bool SavePlayerInfo();
	bool LoadPlayerInfo();

	bool SaveLevelData(Board* board, CardSlotManager* manager);
	bool LoadLevelData(Board* board, CardSlotManager* manager);
	bool DeleteLevelData(Board* board);

private:
	// 实际序列化逻辑（须为成员：Board 仅 friend 本类，自由函数无私有成员访问权）。
	// 上面的公有接口只做一层 try/catch 包裹，详见 .cpp 的“异常安全边界”。
	static bool SavePlayerInfoImpl();
	static bool LoadPlayerInfoImpl();
	static bool SaveLevelDataImpl(Board* board, CardSlotManager* manager);
	static bool LoadLevelDataImpl(Board* board, CardSlotManager* manager);
};

#endif