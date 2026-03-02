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
};


#endif