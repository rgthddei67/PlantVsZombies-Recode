#include "Card.h"
#include "CardComponent.h"
#include "ColliderComponent.h"
#include "../DeltaTime.h"

Card::Card(PlantType plantType, int sunCost, float cooldown, bool isInChooseCardUI) {
	mObjectType = ObjectType::OBJECT_UI;
    this->mIsInChooseCardUI = isInChooseCardUI;
    SetupComponents(plantType, sunCost, cooldown);
}

void Card::SetupComponents(PlantType plantType, int sunCost, float cooldown) {
    mTransform = AddComponent<TransformComponent>();

    auto displayComponent = AddComponent<CardDisplayComponent>
        (plantType, sunCost, cooldown);
    displayComponent->SetDrawOrder(50);

    AddComponent<CardComponent>(plantType, sunCost, cooldown);
    // 点击组件
    auto collision = AddComponent<ColliderComponent>(Vector(CARD_WIDTH, CARD_HEIGHT));
    collision->isStatic = true;
	collision->isTrigger = true;
    auto clickable = AddComponent<ClickableComponent>();
    clickable->ConsumeEvent = true;

    // 设置名称和标签
    SetName("PlantCard");
    SetTag("Card");
}

void Card::SetTargetPosition(const Vector& target) {
    m_targetPos = target;
    m_isMoving = true;
}

void Card::Update() {
    GameObject::Update(); // 调用基类更新

    if (m_isMoving) {
        auto transform = GetComponent<TransformComponent>();
        if (!transform) return;

        Vector currentPos = transform->GetPosition();
        Vector dir = m_targetPos - currentPos;
        float dist = dir.magnitude();

        if (dist < 0.1f) {
            transform->SetPosition(m_targetPos);
            m_isMoving = false;
            return;
        }

        float step = m_moveSpeed * DeltaTime::GetDeltaTime();
        if (step >= dist) {
            transform->SetPosition(m_targetPos);
            m_isMoving = false;
        }
        else {
            // 使用 normalized() 获取单位方向向量
            Vector move = dir.normalized() * step;
            transform->SetPosition(currentPos + move);
        }
    }
}