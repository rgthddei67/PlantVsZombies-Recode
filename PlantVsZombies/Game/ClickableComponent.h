#pragma once
#ifndef _CLICKABLE_COMPONENT_H
#define _CLICKABLE_COMPONENT_H

#include "Definit.h"
#include <unordered_set>
#include <vector>
#include <functional>

class ColliderComponent;
class GameObject;

/**
 * @brief GameObject 显式拥有的可选点击附件。
 * @details 构造/析构维护稀疏注册表，命中测试始终使用宿主当前唯一 Collider。
 */
class ClickableComponent {
public:
	bool IsClickable = true;    // 是否可点击
	bool ConsumeEvent = true;   // 是否消耗点击事件，阻止更低层对象响应
	bool ChangeCursorOnHover = true;   // 悬停时改变光标

	std::function<void()> onClick;
	std::function<void()> onMouseEnter;
	std::function<void()> onMouseExit;
	std::function<void()> onMouseDown;
	std::function<void()> onMouseUp;

	~ClickableComponent();

	void Update();

	void SetClickArea(const Vector& size);
	void SetClickOffset(const Vector& offset);
	GameObject* GetGameObject() const { return mGameObject; }

	static void ProcessMouseEvents();
	static void ClearProcessedEvents();

private:
	friend class GameObject;

	explicit ClickableComponent(GameObject* owner);

	// 所有 ClickableComponent 的自注册表，避免每帧扫全场 GameObject
	inline static std::vector<ClickableComponent*> s_allClickables;

	// 存储当前帧处理过的点击事件
	inline static std::unordered_set<ClickableComponent*> s_processedEvents;

	// 鼠标状态
	bool mouseOver = false;
	bool mouseDown = false;
	bool prevMouseOver = false;
	GameObject* mGameObject = nullptr; // 非拥有；生命周期严格短于宿主

	// 当前帧是否有鼠标悬停在可点击对象上
	inline static bool s_hoveringClickable = false;
};

#endif
