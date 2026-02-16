#include "AttachmentSystem.h"
#include "Animator.h"
#include <algorithm>
#include <iostream>

// 将SDL颜色转换为glm::vec4
static glm::vec4 ColorToVec4(const SDL_Color& color) {
    return glm::vec4(
        color.r / 255.0f,
        color.g / 255.0f,
        color.b / 255.0f,
        color.a / 255.0f
    );
}

// 将glm::vec4转换为SDL颜色
static SDL_Color Vec4ToColor(const glm::vec4& vec) {
    return SDL_Color{
        static_cast<Uint8>(std::clamp(vec.r * 255.0f, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.g * 255.0f, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.b * 255.0f, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.a * 255.0f, 0.0f, 255.0f))
    };
}

Attachment::Attachment(const std::string& name)
    : mName(name.empty() ? "Attachment_" + std::to_string(reinterpret_cast<uintptr_t>(this)) : name) {
    mEffects.reserve(MAX_EFFECTS_PER_ATTACHMENT);
}

Attachment::~Attachment() {
    // 分离所有效果
    Detach();
}

void Attachment::Update(float deltaTime) {
    if (!mAlive || !mVisible) return;

    // 清理死亡的效果
    CleanupDeadEffects();

    // 更新所有效果
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Update(deltaTime);
        }
    }
}

void Attachment::Draw(SDL_Renderer* renderer, const TransformData& parentTransform) {
    if (!mAlive || !mVisible || !renderer) return;

    // 计算最终变换
    TransformData finalTransform = parentTransform.Combine(mTransform);

    // 绘制所有效果
    for (auto& effectInfo : mEffects) {
        if (!effectInfo.IsValid() || effectInfo.effect->IsAttached() != mAttached) {
            continue;
        }

        // 如果父节点隐藏且设置了不绘制，则跳过
        if (!mVisible && effectInfo.dontDrawIfParentHidden) {
            continue;
        }

        // 计算效果的最终变换
        TransformData effectTransform = effectInfo.GetFinalTransform(finalTransform);

        // 对于Animator效果，获取其当前的Alpha值并应用
        if (effectInfo.effect->GetEffectType() == EffectType::EFFECT_ANIMATION) {
            if (auto animator = std::dynamic_pointer_cast<Animator>(effectInfo.effect)) {
                // 获取Animator的当前Alpha值
                float animatorAlpha = animator->GetAlpha();

                // 保存Animator的原始Alpha
                float originalAlpha = animatorAlpha;

                // 将Attachment的mAlpha应用到Animator（作为乘数）
                float combinedAlpha = originalAlpha * mAlpha;
                animator->SetAlpha(combinedAlpha);

                // 绘制效果
                effectInfo.effect->Draw(renderer, effectTransform);

                // 恢复Animator的原始Alpha（不改变Animator内部状态）
                animator->SetAlpha(originalAlpha);

                continue; // 已处理，继续下一个效果
            }
        }

        // 对于非Animator效果，正常绘制
        effectInfo.effect->Draw(renderer, effectTransform);
    }
}

void Attachment::SetPosition(const Vector& position) {
    mTransform.SetPosition(position);
}

void Attachment::SetPosition(float x, float y) {
    mTransform.SetPosition(x, y);
}

void Attachment::SetScale(const Vector& scale) {
    mTransform.SetScale(scale);
}

void Attachment::SetScale(float scale) {
    mTransform.SetScale(scale);
}

void Attachment::SetScale(float sx, float sy) {
    mTransform.SetScale(sx, sy);
}

void Attachment::SetRotation(float rotation) {
    mTransform.SetRotation(rotation);
}

void Attachment::OverrideColor(const SDL_Color& color) {
    mColorOverride = color;
    mHasColorOverride = true;

    // 传播颜色到所有效果（除非设置了不传播）
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid() && !effectInfo.dontPropagateColor) {
            effectInfo.effect->OverrideColor(color);
        }
    }
}

void Attachment::SetAlpha(float alpha) {
    mAlpha = std::clamp(alpha, 0.0f, 1.0f);

    // 传播透明度到所有效果
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->SetAlpha(mAlpha);
        }
    }
}

void Attachment::SetVisible(bool visible) {
    mVisible = visible;

    // 传播可见性到所有效果（除非设置了不继承可见性）
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid() && effectInfo.inheritVisibility) {
            effectInfo.effect->SetVisible(visible);
        }
    }
}

void Attachment::PropagateColor(const SDL_Color& color,
    bool enableAdditive, const SDL_Color& additiveColor,
    bool enableOverlay, const SDL_Color& overlayColor) {
    mColorOverride = color;
    mHasColorOverride = true;

    // 传播颜色到所有效果（除非设置了不传播）
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid() && !effectInfo.dontPropagateColor) {
            effectInfo.effect->PropagateColor(color, enableAdditive, additiveColor,
                enableOverlay, overlayColor);
        }
    }
}

void Attachment::SetTransformMatrix(const glm::mat3& matrix) {
    mTransform.matrix = matrix;

    // 从矩阵中提取位置、缩放和旋转
    mTransform.position = glm::vec2(matrix[2][0], matrix[2][1]);
    mTransform.scale.x = glm::length(glm::vec2(matrix[0][0], matrix[0][1]));
    mTransform.scale.y = glm::length(glm::vec2(matrix[1][0], matrix[1][1]));

    if (mTransform.scale.x > 0 && mTransform.scale.y > 0) {
        mTransform.rotation = atan2(matrix[0][1] / mTransform.scale.y,
            matrix[0][0] / mTransform.scale.x);
    }
}

void Attachment::Die() {
    // 标记为死亡
    mAlive = false;

    // 销毁所有效果
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Die();
        }
    }

    // 清空效果列表
    mEffects.clear();
}

void Attachment::Detach() {
    // 分离所有效果
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Detach();
        }
    }

    // 清空效果列表
    mEffects.clear();

    // 标记为未附加
    mAttached = false;
}

bool Attachment::AddEffect(std::shared_ptr<IAttachmentEffect> effect,
    const Vector& offset,
    const Vector& scale,
    float rotation) {
    if (!effect || IsFull()) {
        return false;
    }

    // 检查效果是否已经附加
    if (effect->IsAttached()) {
        std::cerr << "Warning: Effect is already attached to another attachment!" << std::endl;
        return false;
    }

    // 设置偏移变换
    TransformData offsetTransform;
    offsetTransform.SetPosition(offset);
    offsetTransform.SetScale(scale);
    offsetTransform.SetRotation(rotation);

    // 设置效果为附加状态
    effect->SetAttached(true);

    // 创建效果信息
    AttachmentEffect effectInfo;
    effectInfo.effect = std::move(effect);
    effectInfo.offsetTransform = offsetTransform;

    // 添加到列表
    mEffects.push_back(std::move(effectInfo));
    return true;
}

bool Attachment::AddEffect(std::shared_ptr<IAttachmentEffect> effect,
    float offsetX, float offsetY,
    float scaleX, float scaleY,
    float rotation) {
    return AddEffect(effect,
        Vector(offsetX, offsetY),
        Vector(scaleX, scaleY),
        rotation);
}

bool Attachment::AddAnimator(std::shared_ptr<Animator> animator,
    const Vector& offset,
    const Vector& scale,
    float rotation) {
    if (!animator) {
        return false;
    }

    // Animator应该已经实现了IAttachmentEffect接口
    return AddEffect(std::static_pointer_cast<IAttachmentEffect>(animator),
        offset, scale, rotation);
}

bool Attachment::AddAnimator(std::shared_ptr<Animator> animator,
    float offsetX, float offsetY,
    float scaleX, float scaleY,
    float rotation) {
    return AddAnimator(animator,
        Vector(offsetX, offsetY),
        Vector(scaleX, scaleY),
        rotation);
}

bool Attachment::RemoveEffect(int index) {
    if (index < 0 || index >= static_cast<int>(mEffects.size())) {
        return false;
    }

    auto& effectInfo = mEffects[index];
    if (effectInfo.IsValid()) {
        effectInfo.effect->SetAttached(false);
        effectInfo.effect->Detach();
    }

    mEffects.erase(mEffects.begin() + index);
    return true;
}

bool Attachment::RemoveEffect(std::shared_ptr<IAttachmentEffect> effect) {
    for (auto it = mEffects.begin(); it != mEffects.end(); ++it) {
        if (it->effect == effect) {
            if (it->IsValid()) {
                it->effect->SetAttached(false);
                it->effect->Detach();
            }
            mEffects.erase(it);
            return true;
        }
    }
    return false;
}

void Attachment::RemoveAllEffects() {
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->SetAttached(false);
            effectInfo.effect->Detach();
        }
    }
    mEffects.clear();
}

std::shared_ptr<IAttachmentEffect> Attachment::FindEffect(int index) const {
    if (index >= 0 && index < static_cast<int>(mEffects.size())) {
        return mEffects[index].effect;
    }
    return nullptr;
}

std::shared_ptr<Animator> Attachment::FindFirstAnimator() const {
    for (const auto& effectInfo : mEffects) {
        if (effectInfo.effect && effectInfo.effect->GetEffectType() == EffectType::EFFECT_ANIMATION) {
            return std::dynamic_pointer_cast<Animator>(effectInfo.effect);
        }
    }
    return nullptr;
}

std::vector<std::shared_ptr<Animator>> Attachment::FindAllAnimators() const {
    std::vector<std::shared_ptr<Animator>> animators;
    for (const auto& effectInfo : mEffects) {
        if (effectInfo.effect && effectInfo.effect->GetEffectType() == EffectType::EFFECT_ANIMATION) {
            if (auto animator = std::dynamic_pointer_cast<Animator>(effectInfo.effect)) {
                animators.push_back(animator);
            }
        }
    }
    return animators;
}

void Attachment::CleanupDeadEffects() {
    // 移除死亡或无效的效果
    auto newEnd = std::remove_if(mEffects.begin(), mEffects.end(),
        [](const AttachmentEffect& effectInfo) {
            return !effectInfo.IsValid();
        });

    if (newEnd != mEffects.end()) {
        mEffects.erase(newEnd, mEffects.end());
    }
}

AttachmentSystem::~AttachmentSystem() {
    RemoveAllAttachments();
}

std::shared_ptr<Attachment> AttachmentSystem::CreateAttachment(const std::string& name) {
    // 检查名称是否已存在
    if (!name.empty() && HasAttachment(name)) {
        std::cerr << "AttachmentSystem: Attachment with name '" << name << "' already exists!" << std::endl;
        return nullptr;
    }

    auto attachment = std::make_shared<Attachment>(name);
    mActiveAttachments.push_back(attachment);

    if (!name.empty()) {
        mNamedAttachments[name] = attachment;
    }

    return attachment;
}

std::shared_ptr<Attachment> AttachmentSystem::GetAttachment(const std::string& name) {
    auto it = mNamedAttachments.find(name);
    if (it != mNamedAttachments.end()) {
        return it->second;
    }
    return nullptr;
}

bool AttachmentSystem::HasAttachment(const std::string& name) const {
    return mNamedAttachments.find(name) != mNamedAttachments.end();
}

bool AttachmentSystem::RemoveAttachment(const std::string& name) {
    auto it = mNamedAttachments.find(name);
    if (it == mNamedAttachments.end()) {
        return false;
    }

    auto attachment = it->second;

    // 从活动列表中移除
    mActiveAttachments.erase(
        std::remove(mActiveAttachments.begin(), mActiveAttachments.end(), attachment),
        mActiveAttachments.end()
    );

    // 销毁附件
    attachment->Die();

    // 从命名映射中移除
    mNamedAttachments.erase(it);

    return true;
}

void AttachmentSystem::RemoveAllAttachments() {
    for (auto& attachment : mActiveAttachments) {
        if (attachment) {
            attachment->Die();
        }
    }

    mActiveAttachments.clear();
    mNamedAttachments.clear();
}

void AttachmentSystem::UpdateAll(float deltaTime) {
    // 清理死亡附件
    CleanupDeadAttachments();

    // 更新所有附件
    for (auto& attachment : mActiveAttachments) {
        UpdateAttachment(attachment, deltaTime);
    }
}

void AttachmentSystem::DrawAll(SDL_Renderer* renderer) {
    if (!renderer) return;

    TransformData identity; // 单位变换

    for (auto& attachment : mActiveAttachments) {
        if (attachment && attachment->IsAlive()) {
            attachment->Draw(renderer, identity);
        }
    }
}

void AttachmentSystem::CleanupDeadAttachments() {
    // 从活动列表中移除死亡附件
    auto newEnd = std::remove_if(mActiveAttachments.begin(), mActiveAttachments.end(),
        [](const std::shared_ptr<Attachment>& attachment) {
            return !attachment || !attachment->IsAlive();
        });

    // 从命名映射中移除死亡附件
    for (auto it = newEnd; it != mActiveAttachments.end(); ++it) {
        for (auto mapIt = mNamedAttachments.begin(); mapIt != mNamedAttachments.end(); ) {
            if (mapIt->second == *it) {
                mapIt = mNamedAttachments.erase(mapIt);
            }
            else {
                ++mapIt;
            }
        }
    }

    if (newEnd != mActiveAttachments.end()) {
        mActiveAttachments.erase(newEnd, mActiveAttachments.end());
    }
}

std::vector<std::string> AttachmentSystem::GetAllAttachmentNames() const {
    std::vector<std::string> names;
    names.reserve(mNamedAttachments.size());

    for (const auto& pair : mNamedAttachments) {
        names.push_back(pair.first);
    }

    return names;
}

void AttachmentSystem::UpdateAttachment(std::shared_ptr<Attachment>& attachment, float deltaTime) {
    if (attachment && attachment->IsAlive()) {
        attachment->Update(deltaTime);
    }
}