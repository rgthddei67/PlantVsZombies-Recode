#include "SceneManager.h"
#include "../Graphics.h"
#include "../Logger.h"

SceneManager& SceneManager::GetInstance() {
	static SceneManager instance;
	return instance;
}

void SceneManager::ClearCurrentScene() {
	if (currentScene_) {
		currentScene_->OnExit();
		currentScene_.reset();
	}
}

bool SceneManager::SwitchTo(const std::string& name) {
	auto it = scenes_.find(name);
	if (it == scenes_.end()) {
		LOG_ERROR("SceneManager") << "场景未注册: " << name;
		return false;
	}

	// 退出当前场景
	if (currentScene_) {
		currentScene_->OnExit();
		currentScene_.reset();
	}

	// 创建并进入新场景
	auto newScene = it->second();
	newScene->OnEnter();
	currentScene_ = std::move(newScene);
	LOG_INFO("SceneManager") << "切换到场景: " << name;
	return true;
}

void SceneManager::Update() {
	if (currentScene_) {
		currentScene_->Update();
	}
}

void SceneManager::Draw(Graphics* g) {
	if (currentScene_) {
		currentScene_->Draw(g);
	}
}

void SceneManager::DrawWorldOverlay(Graphics* g) {
	if (currentScene_) {
		currentScene_->DrawWorldOverlay(g);
	}
}

Scene* SceneManager::GetCurrentScene() const {
	return currentScene_.get();
}

bool SceneManager::IsEmpty() const {
	return currentScene_ == nullptr;
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
