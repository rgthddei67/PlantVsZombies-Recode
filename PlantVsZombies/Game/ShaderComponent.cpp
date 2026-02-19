#include "ShadowComponent.h"
#include "../ResourceManager.h"
#include "GameObject.h"
#include <algorithm>
#include <iostream>
#include "Plant/Plant.h"
#include "Zombie/Zombie.h"

ShadowComponent::ShadowComponent(SDL_Texture* shadowTexture,
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

void ShadowComponent::Draw(SDL_Renderer* renderer) {
    if (!renderer) return;

    auto gameObject = GetGameObject();
    if (!gameObject) return;

    // 计算阴影位置（在物体下方，加上偏移）
	ObjectType type = gameObject->GetObjectType();
    Vector shadowPos = Vector(0, 0);
    Vector transform = gameObject->GetComponent<TransformComponent>()->GetWorldPosition();
    
    if (type == ObjectType::OBJECT_PLANT)
    {
        if (auto plant = std::dynamic_pointer_cast<Plant>(gameObject))
        {
            shadowPos = transform + mOffset;
        }
    }
    else if (type == ObjectType::OBJECT_ZOMBIE)
    {
        if (auto zombie = std::dynamic_pointer_cast<Zombie>(gameObject))
        {
            shadowPos = transform + mOffset;
        }
    }
    else
    {
		std::cout << 
            "ShadowComponent: GameObject has no TransformComponent or"
            " Plant & Zombie & Bullet class." 
            << std::endl;
        return;
    }
    
    SDL_Color oldColor;
    SDL_GetRenderDrawColor(renderer, &oldColor.r, &oldColor.g, &oldColor.b, &oldColor.a);
    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);

    // 设置透明度和混合模式
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    Uint8 shadowAlpha = static_cast<Uint8>(mAlpha * 255);

    if (mShadowTexture) {
        int texWidth, texHeight;
        SDL_QueryTexture(mShadowTexture, NULL, NULL, &texWidth, &texHeight);

        SDL_Rect dstRect = {
            static_cast<int>(shadowPos.x - texWidth * mScale.x / 2),
            static_cast<int>(shadowPos.y - texHeight * mScale.y / 2),
            static_cast<int>(texWidth * mScale.x),
            static_cast<int>(texHeight * mScale.y)
        };

        SDL_SetTextureAlphaMod(mShadowTexture, shadowAlpha);
        SDL_SetTextureColorMod(mShadowTexture, 255, 255, 255); 
        SDL_RenderCopy(renderer, mShadowTexture, NULL, &dstRect);
        SDL_SetTextureAlphaMod(mShadowTexture, 255);
    }

    // 恢复渲染状态
    SDL_SetRenderDrawColor(renderer, oldColor.r, oldColor.g, oldColor.b, oldColor.a);
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
}