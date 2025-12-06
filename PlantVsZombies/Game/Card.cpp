#include "Card.h"
#include "CardComponent.h"
#include "ColliderComponent.h"

Card::Card(PlantType plantType, int sunCost, float cooldown) {
    SetupComponents(plantType, sunCost, cooldown);
}

void Card::SetupComponents(PlantType plantType, int sunCost, float cooldown) {
    AddComponent<TransformComponent>();
    AddComponent<CardDisplayComponent>(plantType, sunCost, cooldown);
    AddComponent<CardComponent>(plantType, sunCost, cooldown);
    // 点击组件
    auto collision = AddComponent<ColliderComponent>(Vector(CARD_WIDTH, CARD_HEIGHT), ColliderType::BOX);
    collision->isStatic = true;
	collision->isTrigger = true;
    auto clickable = AddComponent<ClickableComponent>();
    clickable->ConsumeEvent = true;

    // 设置名称和标签
    SetName("PlantCard");
    SetTag("Card");
}