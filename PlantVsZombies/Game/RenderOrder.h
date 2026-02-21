#pragma once
#ifndef _RENDER_ORDER_H
#define _RENDER_ORDER_H

enum RenderLayer{
    // 背景层
    LAYER_BACKGROUND = -10000,

    // 游戏对象层
    LAYER_GAME_OBJECT = 0,

	// 游戏植物层
	LAYER_GAME_PLANT = 10000,

    // 游戏僵尸层
    LAYER_GAME_ZOMBIE = 20000,

    // 游戏子弹层
	LAYER_GAME_BULLET = 30000,

    // UI层
    LAYER_UI = 40000,

	// 游戏物品层
    LAYER_GAME_COIN = 50000,

    // 效果层
    LAYER_EFFECTS = 80000,

    // 调试层
    LAYER_DEBUG = 100000
};

#endif