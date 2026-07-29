#pragma once
#ifndef _GAME_SELECT_SCENE_H
#define _GAME_SELECT_SCENE_H

#include "Scene.h"
#include <vector>
#include <memory>

// 挑战模式风格的生存关卡选择界面：
// 羊皮纸背景 + 顶部标题 + 无尽模式卡片网格 + 左下「返回菜单」按钮。
// 卡片点击只登记待进入关卡，由 Update 在 UI 回调结束后统一切换到 GameScene。
class GameSelectScene : public Scene {
private:
	std::shared_ptr<Button> mBackMenuButton;
	std::vector<std::shared_ptr<Button>> mCards;   // 当前启用的关卡卡片
	bool mReadyToSwitchMainMenu = false;
	int  mPendingEnterLevel = -1;                  // 待进入的关卡号(>=0 时 Update 进 GameScene)

public:
	void OnEnter() override;
	void OnExit() override;
	void Update() override;

protected:
	void BuildDrawCommands() override;
};

#endif
