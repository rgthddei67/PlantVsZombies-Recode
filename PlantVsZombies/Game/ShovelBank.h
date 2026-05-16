#pragma once
#ifndef _SHOVELBANK_H
#define _SHOVELBANK_H

#include "GameObject.h"

class Board;
struct Texture;

class ShovelBank : public GameObject {
public:
	ShovelBank(Board* board);
	void Start() override;
	void Draw(Graphics* g) override;

private:
	Board* mBoard;
	const Texture* mTexture;
	bool mWasShovelActive = false;
};

#endif
