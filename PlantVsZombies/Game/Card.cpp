#include "Card.h"
#include "CardComponent.h"

Card::Card(PlantType plantType, int sunCost, float cooldown) {
    SetupComponents(plantType, sunCost, cooldown);
}

void Card::SetupComponents(PlantType plantType, int sunCost, float cooldown) {
    AddComponent<TransformComponent>();
    AddComponent<CardComponent>(plantType, sunCost, cooldown);
    AddComponent<CardDisplayComponent>(plantType, sunCost, cooldown);

    // 点击组件
    AddComponent<ColliderComponent>(Vector(CARD_WIDTH, CARD_HEIGHT), ColliderType::BOX);
    auto clickable = AddComponent<ClickableComponent>();
    clickable->ConsumeEvent = true;

    // 设置名称和标签
    SetName("PlantCard");
    SetTag("Card");
}