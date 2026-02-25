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

    // 获取鼠标屏幕坐标和世界坐标
    Vector mouseScreen = input.GetMousePosition();
    Vector mouseWorld = input.GetMouseWorldPosition();

    // 清空上一帧的处理记录
    ClearProcessedEvents();

    s_hoveringClickable = false;
    // 收集所有鼠标位置下的可点击对象（使用世界坐标）
    auto& manager = GameObjectManager::GetInstance();
    auto allObjects = manager.GetAllGameObjects();

    std::vector<std::pair<std::shared_ptr<GameObject>, ClickableComponent*>> clickableObjects;

    for (size_t i = 0; i < allObjects.size(); i++)
    {
        auto obj = allObjects[i];
        if (!obj->IsActive()) continue;

        auto clickable = obj->GetComponent<ClickableComponent>();
        if (!clickable || !clickable->IsClickable) continue;

        auto collider = clickable->mCollider;
        if (!collider || !collider->mEnabled) continue;

        Vector testPoint = obj->mIsUI ? mouseScreen : mouseWorld;

        if (collider->ContainsPoint(testPoint)) {
            clickableObjects.emplace_back(obj, clickable.get());

            if (clickable->ChangeCursorOnHover) {
                s_hoveringClickable = true;
            }
        }

        // 使用转换后的世界坐标进行点包含测试
        if (collider->ContainsPoint(testPoint)) {
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
    for (size_t i = 0; i < clickableObjects.size(); i++)
    {
        auto pair = clickableObjects[i];
        auto clickable = pair.second;
        bool wasHovered = clickable->mouseOver;
        pair.second->mouseOver = true;

        if (!wasHovered && clickable->ChangeCursorOnHover) {
            CursorManager::GetInstance().IncrementHoverCount();
        }

        if (s_processedEvents.find(clickable) != s_processedEvents.end()) {
            continue;
        }

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

        if (processed && clickable->ConsumeEvent) {
            s_processedEvents.insert(clickable);
            for (size_t j = 0; j < clickableObjects.size(); j++)
            {
                auto remainingPair = clickableObjects[j];
                if (remainingPair.second != clickable) {
                    s_processedEvents.insert(remainingPair.second);
                }
            }
            break;
        }

        if (processed) {
            s_processedEvents.insert(clickable);
        }
        clickableObjects[i].second->Update();
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