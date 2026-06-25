#pragma once
#ifndef _GAME_SELECT_SCENE_H
#define _GAME_SELECT_SCENE_H

#include "Scene.h"
#include <vector>
#include <memory>

// 挑战模式风格的「选择关卡」界面（纯视觉脚手架）：
// 羊皮纸背景 + 顶部标题 + 6+3 关卡卡片网格 + 左下「返回菜单」按钮。
// 卡片为占位（可悬停高亮、点击仅打日志，不跳转）；入口暂不接，仅注册到 SceneManager。
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
