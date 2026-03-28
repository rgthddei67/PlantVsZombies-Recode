#include "ShovelBank.h"
#include "Board.h"
#include "ClickableComponent.h"
#include "TransformComponent.h"
#include "GameObjectManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../Graphics.h"

ShovelBank::ShovelBank(Board* board)
	: mBoard(board)
{
	mTexture = ResourceManager::GetInstance()
		.GetTexture(ResourceKeys::Textures::IMAGE_SHOVELBANK);
	mIsUI = true;
	SetRenderOrder(LAYER_UI + 49000);

	AddComponent<TransformComponent>(850.0f, 30.0f);
}

void ShovelBank::Start()
{
	GameObject::Start();

	// 添加 ClickableComponent
	auto clickComponent = AddComponent<ClickableComponent>();
	if (clickComponent) {
		clickComponent->IsClickable = true;
		clickComponent->ConsumeEvent = true;
		clickComponent->SetClickArea(Vector(70.0f, 72.0f));
		clickComponent->SetClickOffset(Vector(-35.0f, -36.0f));
		clickComponent->onClick = [this]() {
			mBoard->ActivateShovel();
		};
	}
}

void ShovelBank::Draw(Graphics* g)
{
	if (!mTexture || !g) return;

	auto transform = GetComponent<TransformComponent>();
	if (!transform) return;

	Vector pos = transform->GetPosition();
	Vector worldPos = g->ScreenToWorldPosition(pos.x, pos.y);

	float w = static_cast<float>(mTexture->width);
	float h = static_cast<float>(mTexture->height);
	g->DrawTexture(mTexture,
		worldPos.x - w * 0.5f,
		worldPos.y - h * 0.5f,
		w, h);
}
