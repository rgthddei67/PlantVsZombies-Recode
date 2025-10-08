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

    // ����ע��
    template<typename T, typename... Args>
    void RegisterScene(const std::string& name, Args&&... args) {
        scenes_[name] = [=]() -> std::unique_ptr<Scene> {
            auto scene = std::make_unique<T>(std::forward<Args>(args)...);
            scene->name = name;
            return scene;
            };
    }

    // �����л�
    bool SwitchTo(const std::string& name);

    // ѹ�볡������ͣ��ǰ��
    void PushScene(const std::string& name);

    // �����������ָ���һ����
    void PopScene();

    void Update();
    void Draw(SDL_Renderer* renderer);
    void HandleEvent(SDL_Event& event);

    // ��ȡ��ǰ������ָ��
    Scene* GetCurrentScene() const;

    // �ж���û�г���
    bool IsEmpty() const;

    // ����ȫ������
    void SetGlobalData(const std::string& key, const std::string& value);

    // ��ȡȫ������
    std::string GetGlobalData(const std::string& key, const std::string& defaultValue = "") const;

    // �������ȫ������
    void ClearGlobalData();

private:
    SceneManager() = default;
    ~SceneManager() = default;

    std::unordered_map<std::string, std::function<std::unique_ptr<Scene>()>> scenes_;
    std::unordered_map<std::string, std::string> globalData_;
    std::stack<std::unique_ptr<Scene>> sceneStack_;
};

#endif