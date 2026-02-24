#pragma once
#ifndef _SHADOW_COMPONENT_H
#define _SHADOW_COMPONENT_H

#include "Component.h"
#include "TransformComponent.h"
#include "../ResourceKeys.h"
#include "../ResourceManager.h"
#include "Definit.h"
#include <algorithm>

class ShadowComponent : public Component {
private:
    const GLTexture* mShadowTexture = nullptr;
    Vector mOffset = Vector(15, 28);  // 阴影相对于物体位置的偏移
    Vector mScale = Vector(1.0f, 0.75f);  // 阴影的缩放（通常水平拉伸，垂直压缩）
    float mAlpha = 0.9f;            // 阴影透明度

public:
    ShadowComponent(const GLTexture* shadowTexture = nullptr,
        const Vector& offset = Vector(0, 28),
        float alpha = 0.9f);

    ~ShadowComponent() override;

    void Start() override;
    void Draw(Graphics* g) override;

    // 设置阴影纹理
    void SetShadowTexture(const GLTexture* texture) { mShadowTexture = texture; }

    // 设置阴影偏移
    void SetOffset(const Vector& offset) { mOffset = offset; }

    // 设置阴影透明度
    void SetAlpha(float alpha) { mAlpha = std::clamp(alpha, 0.0f, 1.0f); }

    // 设置阴影缩放
    void SetScale(const Vector& scale) { mScale = scale; }
};

#endif