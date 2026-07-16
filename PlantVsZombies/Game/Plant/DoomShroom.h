#pragma once
#ifndef _H_DOOMSHROOM_H
#define _H_DOOMSHROOM_H

#include "Shroom.h"
#include "../Board.h"

// 毁灭菇：夜晚种下立即充能（anim_explode 全局 19..51 帧、按原版 23fps 播放），
// 第 51 帧（主人指定）引爆——半径 250 圆形全场结算 + 原地留弹坑 180s，随即本体消失。
// 白天种下睡觉（Shroom 基类处理，anim_sleep 活跃区间 52..76，扫不到第 51 帧不会误爆）。
// 充能期间可被啃食（同原版：被啃死则不爆炸）。
class DoomShroom : public Shroom
{
public:
	using Shroom::Shroom;

protected:
	void SetupPlant() override;

private:
	void Explode();
};

#endif
