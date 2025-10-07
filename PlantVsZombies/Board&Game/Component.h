#pragma once
#ifndef _COMPONENT_H
#define _COMPONENT_H

#include "Definit.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <functional>
#include <vector>

class GameObject;

class Component : public std::enable_shared_from_this<Component> {
public:
	std::weak_ptr<GameObject> gameObjectWeak;   // 不能shared_ptr 防止循环引用(GameObject)
    bool enabled = true;

    virtual ~Component() = default;

    // 生命周期方法
    virtual void Start() {}           // 组件开始时调用
    virtual void Update() {}          // 每帧更新
    virtual void OnDestroy() {}       // 组件销毁时调用

    // 获取 GameObject（如果还存在）
    std::shared_ptr<GameObject> GetGameObject() const;

    // 设置所属游戏对象
    void SetGameObject(std::shared_ptr<GameObject> obj);

    // 绘制方法（可选）
    virtual void Draw(SDL_Renderer* renderer) {}
};

#endif