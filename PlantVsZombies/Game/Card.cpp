#include "Card.h"
#include "CardComponent.h"
#include "ColliderComponent.h"

Card::Card(PlantType plantType, int sunCost, float cooldown) {
	mObjectType = ObjectType::OBJECT_UI;
    SetupComponents(plantType, sunCost, cooldown);
}

void Card::SetupComponents(PlantType plantType, int sunCost, float cooldown) {
    AddComponent<TransformComponent>();
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