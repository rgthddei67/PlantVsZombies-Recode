#pragma once
#ifndef _GAMEINFOSAVER_H
#define _GAMEINFOSAVER_H
#include "FileManager.h"

class Board;

class GameInfoSaver {
public:
	bool SavePlayerInfo();
	bool LoadPlayerInfo();
	
	bool SaveLevelData(Board* board);
	bool LoadLevelData(Board* board);
};


#endif