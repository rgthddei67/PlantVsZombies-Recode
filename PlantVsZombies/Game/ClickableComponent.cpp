#include "ClickableComponent.h"
#include "ColliderComponent.h"
#include "GameObjectManager.h"
#include "../UI/InputHandler.h"
#include "../GameApp.h"
#include "GameObject.h"

void ClickableComponent::ClearConsumedEvents() {
    s_processedThisFrame.clear();
}

bool ClickableComponent::IsEventConsumedByHigherObject(
    const std::shared_ptr<GameObject>& currentObj,
    const std::vector<std::shared_ptr<GameObject>>& sortedObjects) {

    for (const auto& obj : sortedObjects) {
        if (obj == currentObj) {
            break; // 已经检查到当前对象，之前没有更高层级的对象处理事件
        }

        auto clickable = obj->GetComponent<ClickableComponent>();
        if (clickable && s_processedThisFrame.find(clickable.get()) != s_processedThisFrame.end()) {
            // 这个更高层级的对象已经处理了事件
            return true;
        }
    }
    return false;
}

void ClickableComponent::Start() {
    if (auto gameObject = this->GetGameObject())
    {
        collider = gameObject->GetComponent<ColliderComponent>();
        if (collider == nullptr)
        {
#ifdef _DEBUG
            std::cout << "ClickableComponent::Start:"
                << "没有ColliderComponent组件！自动添加" << std::endl;
#endif
            collider = gameObject->AddComponent<ColliderComponent>(Vector(50, 50));
            collider->isTrigger = true;
        }
        else
        {
            collider->isTrigger = true;
        }
    }
}

void ClickableComponent::Update() {
    eventConsumed = false;
    mouseOver = false; // 每次重新计算

    if (!IsClickable || !collider || !collider->mEnabled) {
        return;
    }

    auto& input = GameAPP::GetInstance().GetInputHandler();
    Vector mousePos = input.GetMousePosition();

    // 先收集鼠标位置下所有可点击对象
    auto currentGameObject = GetGameObject();
    if (!currentGameObject) return;

    auto& manager = GameObjectManager::GetInstance();
    auto allObjects = manager.GetAllGameObjects();

    std::vector<std::shared_ptr<GameObject>> clickableObjectsAtMouse;

    for (auto& obj : allObjects) {
        if (!obj->IsActive()) continue;

        auto clickable = obj->GetComponent<ClickableComponent>();
        if (!clickable || !clickable->IsClickable) continue;

        auto objCollider = obj->GetComponent<ColliderComponent>();
        if (!objCollider || !objCollider->mEnabled) continue;

        if (objCollider->ContainsPoint(mousePos)) {
            clickableObjectsAtMouse.push_back(obj);

            // 同时检查悬停状态
            if (obj == currentGameObject) {
                mouseOver = true;
            }
        }
    }

    // 处理鼠标进入/离开事件
    static bool prevMouseOver = false;
    if (mouseOver && !prevMouseOver) {
        if (onMouseEnter) onMouseEnter();
    }
    else if (!mouseOver && prevMouseOver) {
        if (onMouseExit) onMouseExit();
        mouseDown = false;
    }
    prevMouseOver = mouseOver;

    // 只有当鼠标悬停时才处理点击
    if (mouseOver && !clickableObjectsAtMouse.empty()) {
        // 按渲染顺序降序排序（高的在前）
        std::sort(clickableObjectsAtMouse.begin(), clickableObjectsAtMouse.end(),
            [](const std::shared_ptr<GameObject>& a, const std::shared_ptr<GameObject>& b) {
                return a->GetRenderOrder() > b->GetRenderOrder();
            });

        // 检查是否有更高层级的对象已经处理了事件
        bool eventBlocked = false;
        for (const auto& obj : clickableObjectsAtMouse) {
            if (obj == currentGameObject) {
                break; // 已经检查到当前对象
            }

            auto clickable = obj->GetComponent<ClickableComponent>();
            if (clickable && s_processedThisFrame.find(clickable.get()) != s_processedThisFrame.end()) {
                eventBlocked = true;
                break;
            }
        }

        if (!eventBlocked) {
            // 处理点击事件
            if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
                mouseDown = true;
                if (onMouseDown) onMouseDown();
            }

            if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT) && mouseDown) {
                mouseDown = false;
                if (onClick) onClick();
                if (onMouseUp) onMouseUp();

                // 如果设置了ConsumeEvent，则标记为已处理
                if (ConsumeEvent) {
                    s_processedThisFrame.insert(this);
                    eventConsumed = true;
                }
            }
        }
        else {
            // 事件被阻塞，重置鼠标状态
            if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT)) {
                mouseDown = false;
            }
        }
    }
    else {
        if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT)) {
            mouseDown = false;
        }
    }
}

void ClickableComponent::SetClickArea(const Vector& size) {
    if (collider) {
        collider->size = size;
    }
}

void ClickableComponent::SetClickOffset(const Vector& offset) {
    if (collider) {
        collider->offset = offset;
    }
}