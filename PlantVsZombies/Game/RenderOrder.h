#pragma once
#ifndef _RENDER_ORDER_H
#define _RENDER_ORDER_H

enum RenderLayer{
    // 背景层
    LAYER_BACKGROUND = -1000,

    // 游戏对象层
    LAYER_GAME_OBJECT = 0,

    // 效果层
    LAYER_EFFECTS = 100,

    // UI层
    LAYER_UI = 1000,

    // 调试层
    LAYER_DEBUG = 2000
};

#endif