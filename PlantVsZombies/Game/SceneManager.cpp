#include "SceneManager.h"
#include <iostream>

SceneManager& SceneManager::GetInstance() {
    static SceneManager instance;
    return instance;
}

bool SceneManager::SwitchTo(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        std::cerr << "����δע��: " << name << std::endl;
        return false;
    }

    // �˳���ǰ����
    if (!sceneStack_.empty()) {
        sceneStack_.top()->OnExit();
        sceneStack_.pop();
    }

    // �����������³���
    auto newScene = it->second();
    newScene->OnEnter();
    sceneStack_.push(std::move(newScene));

    std::cout << "�л�������: " << name << std::endl;
    return true;
}

void SceneManager::PushScene(const std::string& name) {
    auto it = scenes_.find(name);
    if (it == scenes_.end()) {
        std::cerr << "����δע��: " << name << std::endl;
        return;
    }

    // ��ͣ��ǰ������������OnExit��
    // �����������³���
    auto newScene = it->second();
    newScene->OnEnter();
    sceneStack_.push(std::move(newScene));
}

void SceneManager::PopScene() {
    if (sceneStack_.empty()) return;

    sceneStack_.top()->OnExit();
    sceneStack_.pop();

    // �ָ���һ������
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