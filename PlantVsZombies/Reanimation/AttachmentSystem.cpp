#include "AttachmentSystem.h"
#include "Animator.h"
#include <algorithm>
#include <iostream>

// 灏哠DL棰滆壊杞崲涓篻lm::vec4
static glm::vec4 ColorToVec4(const SDL_Color& color) {
    return glm::vec4(
        color.r,
        color.g,
        color.b,
        color.a
    );
}

// 灏唃lm::vec4杞崲涓篠DL棰滆壊
static SDL_Color Vec4ToColor(const glm::vec4& vec) {
    return SDL_Color{
        static_cast<Uint8>(std::clamp(vec.r, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.g, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.b, 0.0f, 255.0f)),
        static_cast<Uint8>(std::clamp(vec.a, 0.0f, 255.0f))
    };
}

Attachment::Attachment(const std::string& name)
    : mName(name.empty() ? "Attachment_" + std::to_string(reinterpret_cast<uintptr_t>(this)) : name) {
    mEffects.reserve(MAX_EFFECTS_PER_ATTACHMENT);
}

Attachment::~Attachment() {
    // 鍒嗙鎵€鏈夋晥鏋?
    Detach();
}

void Attachment::Update(float deltaTime) {
    if (!mAlive || !mVisible) return;

    // 娓呯悊姝讳骸鐨勬晥鏋?
    CleanupDeadEffects();

    // 鏇存柊鎵€鏈夋晥鏋?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Update(deltaTime);
        }
    }
}

void Attachment::Draw(SDL_Renderer* renderer, const TransformData& parentTransform) {
    if (!mAlive || !mVisible || !renderer) return;

    // 璁＄畻鏈€缁堝彉鎹?
    TransformData finalTransform = parentTransform.Combine(mTransform);

    // 缁樺埗鎵€鏈夋晥鏋?
    for (auto& effectInfo : mEffects) {
        if (!effectInfo.IsValid() || effectInfo.effect->IsAttached() != mAttached) {
            continue;
        }

        // 濡傛灉鐖惰妭鐐归殣钘忎笖璁剧疆浜嗕笉缁樺埗锛屽垯璺宠繃
        if (!mVisible && effectInfo.dontDrawIfParentHidden) {
            continue;
        }

        // 璁＄畻鏁堟灉鐨勬渶缁堝彉鎹?
        TransformData effectTransform = effectInfo.GetFinalTransform(finalTransform);

        // 瀵逛簬Animator鏁堟灉锛岃幏鍙栧叾褰撳墠鐨凙lpha鍊煎苟搴旂敤
        if (effectInfo.effect->GetEffectType() == EffectType::EFFECT_ANIMATION) {
            if (auto animator = std::dynamic_pointer_cast<Animator>(effectInfo.effect)) {
                // 鑾峰彇Animator鐨勫綋鍓岮lpha鍊?
                float animatorAlpha = animator->GetAlpha();

                // 淇濆瓨Animator鐨勫師濮婣lpha
                float originalAlpha = animatorAlpha;

                // 灏咥ttachment鐨刴Alpha搴旂敤鍒癆nimator锛堜綔涓轰箻鏁帮級
                float combinedAlpha = originalAlpha * mAlpha;
                animator->SetAlpha(combinedAlpha);

                // 缁樺埗鏁堟灉
                effectInfo.effect->Draw(renderer, effectTransform);

                // 鎭㈠Animator鐨勫師濮婣lpha锛堜笉鏀瑰彉Animator鍐呴儴鐘舵€侊級
                animator->SetAlpha(originalAlpha);

                continue; // 宸插鐞嗭紝缁х画涓嬩竴涓晥鏋?
            }
        }

        // 瀵逛簬闈濧nimator鏁堟灉锛屾甯哥粯鍒?
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

    // 浼犳挱棰滆壊鍒版墍鏈夋晥鏋滐紙闄ら潪璁剧疆浜嗕笉浼犳挱锛?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid() && !effectInfo.dontPropagateColor) {
            effectInfo.effect->OverrideColor(color);
        }
    }
}

void Attachment::SetAlpha(float alpha) {
    mAlpha = std::clamp(alpha, 0.0f, 1.0f);

    // 浼犳挱閫忔槑搴﹀埌鎵€鏈夋晥鏋?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->SetAlpha(mAlpha);
        }
    }
}

void Attachment::SetVisible(bool visible) {
    mVisible = visible;

    // 浼犳挱鍙鎬у埌鎵€鏈夋晥鏋滐紙闄ら潪璁剧疆浜嗕笉缁ф壙鍙鎬э級
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

    // 浼犳挱棰滆壊鍒版墍鏈夋晥鏋滐紙闄ら潪璁剧疆浜嗕笉浼犳挱锛?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid() && !effectInfo.dontPropagateColor) {
            effectInfo.effect->PropagateColor(color, enableAdditive, additiveColor,
                enableOverlay, overlayColor);
        }
    }
}

void Attachment::SetTransformMatrix(const glm::mat3& matrix) {
    mTransform.matrix = matrix;

    // 浠庣煩闃典腑鎻愬彇浣嶇疆銆佺缉鏀惧拰鏃嬭浆
    mTransform.position = glm::vec2(matrix[2][0], matrix[2][1]);
    mTransform.scale.x = glm::length(glm::vec2(matrix[0][0], matrix[0][1]));
    mTransform.scale.y = glm::length(glm::vec2(matrix[1][0], matrix[1][1]));

    if (mTransform.scale.x > 0 && mTransform.scale.y > 0) {
        mTransform.rotation = atan2(matrix[0][1] / mTransform.scale.y,
            matrix[0][0] / mTransform.scale.x);
    }
}

void Attachment::Die() {
    // 鏍囪涓烘浜?
    mAlive = false;

    // 閿€姣佹墍鏈夋晥鏋?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Die();
        }
    }

    // 娓呯┖鏁堟灉鍒楄〃
    mEffects.clear();
}

void Attachment::Detach() {
    // 鍒嗙鎵€鏈夋晥鏋?
    for (auto& effectInfo : mEffects) {
        if (effectInfo.IsValid()) {
            effectInfo.effect->Detach();
        }
    }

    // 娓呯┖鏁堟灉鍒楄〃
    mEffects.clear();

    // 鏍囪涓烘湭闄勫姞
    mAttached = false;
}

bool Attachment::AddEffect(std::shared_ptr<IAttachmentEffect> effect,
    const Vector& offset,
    const Vector& scale,
    float rotation) {
    if (!effect || IsFull()) {
        return false;
    }

    // 妫€鏌ユ晥鏋滄槸鍚﹀凡缁忛檮鍔?
    if (effect->IsAttached()) {
        std::cerr << "Warning: Effect is already attached to another attachment!" << std::endl;
        return false;
    }

    // 璁剧疆鍋忕Щ鍙樻崲
    TransformData offsetTransform;
    offsetTransform.SetPosition(offset);
    offsetTransform.SetScale(scale);
    offsetTransform.SetRotation(rotation);

    // 璁剧疆鏁堟灉涓洪檮鍔犵姸鎬?
    effect->SetAttached(true);

    // 鍒涘缓鏁堟灉淇℃伅
    AttachmentEffect effectInfo;
    effectInfo.effect = std::move(effect);
    effectInfo.offsetTransform = offsetTransform;

    // 娣诲姞鍒板垪琛?
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

    // Animator搴旇宸茬粡瀹炵幇浜咺AttachmentEffect鎺ュ彛
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
    // 绉婚櫎姝讳骸鎴栨棤鏁堢殑鏁堟灉
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
    // 妫€鏌ュ悕绉版槸鍚﹀凡瀛樺湪
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

    // 浠庢椿鍔ㄥ垪琛ㄤ腑绉婚櫎
    mActiveAttachments.erase(
        std::remove(mActiveAttachments.begin(), mActiveAttachments.end(), attachment),
        mActiveAttachments.end()
    );

    // 閿€姣侀檮浠?
    attachment->Die();

    // 浠庡懡鍚嶆槧灏勪腑绉婚櫎
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
    // 娓呯悊姝讳骸闄勪欢
    CleanupDeadAttachments();

    // 鏇存柊鎵€鏈夐檮浠?
    for (auto& attachment : mActiveAttachments) {
        UpdateAttachment(attachment, deltaTime);
    }
}

void AttachmentSystem::DrawAll(SDL_Renderer* renderer) {
    if (!renderer) return;

    TransformData identity; // 鍗曚綅鍙樻崲

    for (auto& attachment : mActiveAttachments) {
        if (attachment && attachment->IsAlive()) {
            attachment->Draw(renderer, identity);
        }
    }
}

void AttachmentSystem::CleanupDeadAttachments() {
    // 浠庢椿鍔ㄥ垪琛ㄤ腑绉婚櫎姝讳骸闄勪欢
    auto newEnd = std::remove_if(mActiveAttachments.begin(), mActiveAttachments.end(),
        [](const std::shared_ptr<Attachment>& attachment) {
            return !attachment || !attachment->IsAlive();
        });

    // 浠庡懡鍚嶆槧灏勪腑绉婚櫎姝讳骸闄勪欢
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