#pragma once
#ifndef _COMPONENT_H
#define _COMPONENT_H

#include "Definit.h"
#include "../Graphics.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <functional>
#include <vector>

class GameObject;

class Component : public std::enable_shared_from_this<Component> {
protected:
    std::weak_ptr<GameObject> mGameObjectWeak;   // 不能shared_ptr 防止循环引用(GameObject) 因为GameObject也会引用他(一些东西啥的)
    int mDrawOrder = 0;     // 绘制顺序 越大的最先绘制 ( -100 - 100 最好)

public:
    bool mEnabled = true;

    virtual ~Component() = default;

    virtual void Start() {}                         // 组件开始时调用
    virtual void Update() {}                        // 每帧更新
    virtual void OnDestroy() {}                     // 组件销毁时调用
    virtual void Draw(Graphics* g) {}    // 绘制方法

    // 获取 GameObject（如果还存在）
    std::shared_ptr<GameObject> GetGameObject() const;
    void SetDrawOrder(int order) { mDrawOrder = order; }
    int GetDrawOrder() const { return mDrawOrder; }

    // 设置所属游戏对象
    void SetGameObject(std::shared_ptr<GameObject> obj);
};

#endif