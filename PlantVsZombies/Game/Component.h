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

class Component {
protected:
    GameObject* mGameObject = nullptr;   // 由 GameObject 持有 unique_ptr，组件生命周期严格短于宿主，裸指针安全
    int mDrawOrder = 0;     // 绘制顺序 越大的最先绘制 ( -100 - 100 最好)

public:
    bool mEnabled = true;

    virtual ~Component() = default;

    virtual void Start() {}                         // 组件开始时调用
    virtual void Update() {}                        // 每帧更新
    /**
     * @brief 本 Component 是否需要每帧 Update。默认 false。
     *        type-level 静态属性（不依赖实例状态）；运行时 disable 走 mEnabled，与本方法正交。
     *        新增的 Component 派生类如果 override 了 Update() 做实事，必须 override 本方法返回 true。
     */
    virtual bool NeedsUpdate() const { return false; }
    virtual void OnDestroy() {}                     // 组件销毁时调用
    virtual void Draw(Graphics* g) {}    // 绘制方法

    // 获取宿主 GameObject（裸指针，永不为 null —— 在 InitializeComponent 时即赋值）
    GameObject* GetGameObject() const { return mGameObject; }
    void SetDrawOrder(int order) { mDrawOrder = order; }
    int GetDrawOrder() const { return mDrawOrder; }

    // 设置所属游戏对象
    void SetGameObject(GameObject* obj) { mGameObject = obj; }
};

#endif
