#pragma once
#ifndef _H_CAMERA2D_H
#define _H_CAMERA2D_H

#include "./Game/Definit.h"

class Camera2D {
public:
    Vector position;      // 摄像机在世界中的位置（通常对准中心或左上角）
    int viewportWidth;    // 视口宽度
    int viewportHeight;   // 视口高度

    Camera2D(int width, int height)
        : viewportWidth(width), viewportHeight(height) {
    }

    /*
    * 1. 世界坐标（World Coordinates）
        定义：世界坐标是你在游戏世界中放置物体的绝对位置。它可以是很大的范围（比如一个地图有 10000×10000 像素），坐标原点通常设在世界的某个固定点（例如地图左上角或中心）。

        用途：所有可移动的游戏对象（角色、敌人、障碍物、地图块等）都用世界坐标来表示它们的位置。

        特点：世界坐标是连续的，不受窗口大小限制。

        2. 屏幕坐标（Screen Coordinates）
        定义：屏幕坐标是相对于窗口或屏幕的像素坐标，原点通常在窗口左上角，x 轴向右，y 轴向下。范围是 (0, 0) 到 (窗口宽度, 窗口高度)。

        用途：直接用于绘制到屏幕上的一切内容。

        特点：屏幕坐标是离散的像素坐标，范围有限。
        此处为世界坐标 -> 屏幕坐标
    */
    Vector WorldToScreen(const Vector& worldPos) const {
        return Vector(worldPos.x - position.x, worldPos.y - position.y);
    }

    /*
    * 1. 世界坐标（World Coordinates）
        定义：世界坐标是你在游戏世界中放置物体的绝对位置。它可以是很大的范围（比如一个地图有 10000×10000 像素），坐标原点通常设在世界的某个固定点（例如地图左上角或中心）。

        用途：所有可移动的游戏对象（角色、敌人、障碍物、地图块等）都用世界坐标来表示它们的位置。

        特点：世界坐标是连续的，不受窗口大小限制。

        2. 屏幕坐标（Screen Coordinates）
        定义：屏幕坐标是相对于窗口或屏幕的像素坐标，原点通常在窗口左上角，x 轴向右，y 轴向下。范围是 (0, 0) 到 (窗口宽度, 窗口高度)。

        用途：直接用于绘制到屏幕上的一切内容。

        特点：屏幕坐标是离散的像素坐标，范围有限。
        此处为屏幕坐标 -> 世界坐标
    */
    Vector ScreenToWorld(const Vector& screenPos) const {
        return Vector(screenPos.x + position.x, screenPos.y + position.y);
    }

    // 移动摄像机
    void Move(float dx, float dy) {
        position.x += dx;
        position.y += dy;
    }

    // 设置摄像机位置
    void SetPosition(const Vector& pos) {
        position = pos;
    }
};

#endif