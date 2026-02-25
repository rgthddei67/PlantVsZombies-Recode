#include "ShadowComponent.h"
#include "../ResourceManager.h"
#include "GameObject.h"
#include <algorithm>
#include <iostream>
#include "Plant/Plant.h"
#include "Zombie/Zombie.h"

ShadowComponent::ShadowComponent(const GLTexture* shadowTexture,
    const Vector& offset,
    float alpha)
    : mShadowTexture(shadowTexture)
    , mOffset(offset)
    , mAlpha(alpha) {
}

ShadowComponent::~ShadowComponent() {
    mShadowTexture = nullptr;
}

void ShadowComponent::Start() {
    if (!mShadowTexture) {
#ifdef _DEBUG
		std::cout << 
            "ShadowComponent: No shadow texture set, using default texture." << std::endl;
#endif
        mShadowTexture = ResourceManager::GetInstance().GetTexture(
            ResourceKeys::Textures::IMAGE_PLANTSHADOW);
    }
}

void ShadowComponent::Draw(Graphics* g) {
    auto gameObject = GetGameObject();
    if (!gameObject) return;

    // 计算阴影位置（在物体下方，加上偏移）
    ObjectType type = gameObject->GetObjectType();
    Vector shadowPos = Vector(0, 0);
    auto transform = gameObject->GetComponent<TransformComponent>();
    if (!transform) {
        std::cout << "ShadowComponent: GameObject has no TransformComponent." << std::endl;
        return;
    }
    Vector objPos = transform->GetPosition();

    if (type == ObjectType::OBJECT_PLANT || type == ObjectType::OBJECT_ZOMBIE) {
        shadowPos = objPos + mOffset;
    }
    else {
        std::cout << "ShadowComponent: GameObject is not Plant or Zombie." << std::endl;
        return;
    }

    if (!mShadowTexture) return;

    // 纹理尺寸
    float texWidth = static_cast<float>(mShadowTexture->width);
    float texHeight = static_cast<float>(mShadowTexture->height);

    // 计算绘制位置（以阴影位置为中心）
    float drawX = shadowPos.x - texWidth * mScale.x * 0.5f;
    float drawY = shadowPos.y - texHeight * mScale.y * 0.5f;
    float drawW = texWidth * mScale.x;
    float drawH = texHeight * mScale.y;

    // 绘制阴影，使用 tint 的 alpha 控制透明度
    glm::vec4 tint(1.0f, 1.0f, 1.0f, mAlpha);
    g->DrawTexture(mShadowTexture, drawX, drawY, drawW, drawH, 0.0f, tint);
}