#pragma once
#ifndef _SCENEMANAGER_H
#define _SCENEMANAGER_H
#include "Scene.h"
#include <functional>
#include <string>
#include <memory>
#include <SDL2/SDL.h>
#include <unordered_map>
#include <stack>

class SceneManager {
public:
    static SceneManager& GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    // 获取当前Scene的UIManager
    UIManager& GetCurrectSceneUIManager() {
		return GetCurrentScene()->GetUIManager();
	}

    void ClearCurrentScene();

    // 场景注册
    template<typename T, typename... Args>
    void RegisterScene(const std::string& name, Args&&... args) {
        scenes_[name] = [=]() -> std::unique_ptr<Scene> {
            auto scene = std::make_unique<T>(std::forward<Args>(args)...);
            scene->name = name;
            return scene;
            };
    }

    // 场景切换
    bool SwitchTo(const std::string& name);

    // 压入场景（暂停当前）
    void PushScene(const std::string& name);

    // 弹出场景（恢复上一个）
    void PopScene();

    void Update();
    void Draw(SDL_Renderer* renderer);

    // 获取当前场景的指针
    Scene* GetCurrentScene() const;

    // 判断有没有场景
    bool IsEmpty() const;

    // 设置全局数据
    void SetGlobalData(const std::string& key, const std::string& value);

    // 获取全局数据
    std::string GetGlobalData(const std::string& key, const std::string& defaultValue = "") const;

    // 清除所有全局数据
    void ClearGlobalData();

private:
    SceneManager() = default;
    ~SceneManager() = default;

    std::unordered_map<std::string, std::function<std::unique_ptr<Scene>()>> scenes_;
    std::unordered_map<std::string, std::string> globalData_;
    std::stack<std::unique_ptr<Scene>> sceneStack_;
};

#endif