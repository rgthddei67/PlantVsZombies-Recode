#include "ClickableComponent.h"
#include "ColliderComponent.h"
#include "GameObjectManager.h"
#include "../UI/InputHandler.h"
#include "../GameApp.h"
#include "GameObject.h"
#include "../CursorManager.h"
#include <algorithm>

void ClickableComponent::ClearProcessedEvents() {
    s_processedEvents.clear();
}

void ClickableComponent::ProcessMouseEvents() {
    auto& input = GameAPP::GetInstance().GetInputHandler();
    Vector mousePos = input.GetMousePosition();

    // 清空上一帧的处理记录
    ClearProcessedEvents();

    s_hoveringClickable = false;

    // 收集所有鼠标位置下的可点击对象
    auto& manager = GameObjectManager::GetInstance();
    auto allObjects = manager.GetAllGameObjects();

    std::vector<std::pair<std::shared_ptr<GameObject>, ClickableComponent*>> clickableObjects;

    for (auto& obj : allObjects) {
        if (!obj->IsActive()) continue;

        auto clickable = obj->GetComponent<ClickableComponent>();
        if (!clickable || !clickable->IsClickable) continue;
        
		auto collider = clickable->mCollider;
		if (!collider || !collider->mEnabled) continue;

        if (collider->ContainsPoint(mousePos)) {
            clickableObjects.emplace_back(obj, clickable.get());

            if (clickable->ChangeCursorOnHover) {
                s_hoveringClickable = true;
            }
        }
    }

    // 按渲染顺序降序排序（order大的在前）
    std::sort(clickableObjects.begin(), clickableObjects.end(),
        [](const auto& a, const auto& b) {
            return a.first->GetRenderOrder() > b.first->GetRenderOrder();
        });

    // 更新所有对象的鼠标悬停状态 后标记处理过的对象
    for (auto& pair : clickableObjects) {
        auto clickable = pair.second;
        bool wasHovered = clickable->mouseOver;
        pair.second->mouseOver = true;

        // 如果之前没有悬停，现在悬停，增加计数
        if (!wasHovered && clickable->ChangeCursorOnHover) {
            CursorManager::GetInstance().IncrementHoverCount();
        }

        // 如果这个对象的事件已经被处理（由更高层对象消耗），跳过
        if (s_processedEvents.find(clickable) != s_processedEvents.end()) {
            continue;
        }

        // 处理点击事件
        bool processed = false;

        if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
            clickable->mouseDown = true;
            if (clickable->onMouseDown) clickable->onMouseDown();
            processed = true;
        }

        if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT) && clickable->mouseDown) {
            clickable->mouseDown = false;
            if (clickable->onMouseUp) clickable->onMouseUp();
            if (clickable->onClick) clickable->onClick();
            processed = true;
        }

        // 如果对象消耗事件，标记并停止处理后续对象
        if (processed && clickable->ConsumeEvent) {
            s_processedEvents.insert(clickable);
            // 标记所有未处理的对象的事件已被消耗
            for (auto& remainingPair : clickableObjects) {
                if (remainingPair.second != clickable) {
                    s_processedEvents.insert(remainingPair.second);
                }
            }
            break;
        }

        // 如果对象不消耗事件，标记为已处理但继续处理下一个
        if (processed) {
            s_processedEvents.insert(clickable);
        }
    }

    // 调用所有对象的Update来处理鼠标进入/离开事件
    for (auto& pair : clickableObjects) {
        pair.second->Update();
    }
}

void ClickableComponent::Start() {
    if (auto gameObject = this->GetGameObject()) {
        mCollider = gameObject->GetComponent<ColliderComponent>();
        if (!mCollider) {
            mCollider = gameObject->AddComponent<ColliderComponent>(Vector(50, 50));
            mCollider->isTrigger = true;
        }
        else {
            mCollider->isTrigger = true;
        }
    }
}

void ClickableComponent::Update() {
    // 先重置mouseOver状态（会在ProcessMouseEvents中重新设置）
    bool wasMouseOver = mouseOver;
    mouseOver = false;

    // 处理鼠标进入/离开事件
    if (prevMouseOver && !wasMouseOver && onMouseExit) {
        onMouseExit();
        mouseDown = false;
    }
    prevMouseOver = wasMouseOver;
}

void ClickableComponent::SetClickArea(const Vector& size) {
    if (mCollider) mCollider->size = size;
}

void ClickableComponent::SetClickOffset(const Vector& offset) {
    if (mCollider) mCollider->offset = offset;
}