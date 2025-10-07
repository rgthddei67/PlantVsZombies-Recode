#include "ClickableComponent.h"
#include "../UI/InputHandler.h"
#include "../GameApp.h"

void ClickableComponent::Start() {
    if (auto gameObject = this->GetGameObject())
    {
        collider = gameObject->GetComponent<ColliderComponent>();
        if (collider == nullptr)
        {
#ifdef _DEBUG
            std::cout << "ClickableComponent::Start:"
                << "û��ColliderComponent������Զ����" << std::endl;
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

    if (!IsClickable || !collider || !collider->enabled || eventConsumed) {
        return;
    }

    auto& input = GameAPP::GetInstance().GetInputHandler();
    Vector mousePos = input.GetMousePosition();

    bool isOver = collider->ContainsPoint(mousePos);

    // ����������/�뿪�¼�
    if (isOver && !mouseOver) {
        mouseOver = true;
        if (onMouseEnter) onMouseEnter();
    }
    else if (!isOver && mouseOver) {
        mouseOver = false;
        if (onMouseExit) onMouseExit();
        mouseDown = false;
    }

    if (mouseOver) {
        if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
            mouseDown = true;
            if (onMouseDown) onMouseDown();
        }

        if (input.IsMouseButtonReleased(SDL_BUTTON_LEFT) && mouseDown) {
            mouseDown = false;
            if (onClick) onClick();
            if (onMouseUp) onMouseUp();

            if (ConsumeEvent) {
                eventConsumed = true;
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