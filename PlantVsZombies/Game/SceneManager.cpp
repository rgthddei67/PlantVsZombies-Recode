#include "SceneManager.h"
#include <iostream>

SceneManager& SceneManager::GetInstance() {
    static SceneManager instance;
    return instance;
}

bool SceneManager::SwitchTo(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        std::cerr << "场景未注册: " << name << std::endl;
        return false;
    }

    // 退出当前场景
    if (!sceneStack_.empty()) {
        sceneStack_.top()->OnExit();
        sceneStack_.pop();
    }

    // 创建并进入新场景
    auto newScene = it->second();
    newScene->OnEnter();
    sceneStack_.push(std::move(newScene));

    std::cout << "切换到场景: " << name << std::endl;
    return true;
}

void SceneManager::PushScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        std::cerr << "场景未注册: " << name << std::endl;
        return;
    }

    // 暂停当前场景（不调用OnExit）
    // 创建并进入新场景
    auto newScene = it->second();
    newScene->OnEnter();
    sceneStack_.push(std::move(newScene));
}

void SceneManager::PopScene() {
    if (sceneStack_.empty()) return;

    sceneStack_.top()->OnExit();
    sceneStack_.pop();

    // 恢复上一个场景
    if (!sceneStack_.empty()) {
        sceneStack_.top()->OnEnter();
    }
}

void SceneManager::Update() {
    if (!sceneStack_.empty()) {
        sceneStack_.top()->Update();
    }
}

void SceneManager::Draw(SDL_Renderer* renderer) {
    if (!sceneStack_.empty()) {
        sceneStack_.top()->Draw(renderer);
    }
}

void SceneManager::HandleEvent(SDL_Event& event, InputHandler& input) {
    if (!sceneStack_.empty()) {
        sceneStack_.top()->HandleEvent(event, input);
    }
}

Scene* SceneManager::GetCurrentScene() const {
    return sceneStack_.empty() ? nullptr : sceneStack_.top().get();
}

bool SceneManager::IsEmpty() const {
    return sceneStack_.empty();
}

void SceneManager::SetGlobalData(const std::string& key, const std::string& value) {
    globalData_[key] = value;
}

std::string SceneManager::GetGlobalData(const std::string& key, const std::string& defaultValue) const {
    auto it = globalData_.find(key);
    return it != globalData_.end() ? it->second : defaultValue;
}

void SceneManager::ClearGlobalData() {
    globalData_.clear();
}