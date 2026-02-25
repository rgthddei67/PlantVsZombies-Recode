#pragma once
#ifndef _ATTACHMENT_SYSTEM_H
#define _ATTACHMENT_SYSTEM_H

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include "../Game/Definit.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>

class Animator;

// 鏁堟灉绫诲瀷
enum class EffectType {
    EFFECT_NONE = 0,
    EFFECT_ANIMATION     // 鍔ㄧ敾鏁堟灉
};

// 鍙樻崲淇℃伅缁撴瀯
struct TransformData {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 scale = glm::vec2(1.0f, 1.0f);
    float rotation = 0.0f; 
    glm::mat3 matrix = glm::mat3(1.0f);

    TransformData() = default;

    TransformData(const glm::vec2& pos, const glm::vec2& scl, float rot)
        : position(pos), scale(scl), rotation(rot) {
        UpdateMatrix();
    }

    TransformData(const Vector& pos, const Vector& scl, float rot)
        : position(glm::vec2(pos.x, pos.y)),
        scale(glm::vec2(scl.x, scl.y)),
        rotation(rot) {
        UpdateMatrix();
    }

    // 鏇存柊鍙樻崲鐭╅樀
    void UpdateMatrix() {
        matrix = glm::mat3(1.0f);
        matrix[0][0] = scale.x;
        matrix[1][1] = scale.y;

        float c = cos(rotation);
        float s = sin(rotation);
        glm::mat3 rotMat = glm::mat3(c, -s, 0, s, c, 0, 0, 0, 1);
        matrix = matrix * rotMat;

        matrix[2][0] = position.x;
        matrix[2][1] = position.y;
    }

    // 鍙樻崲涓€涓偣
    glm::vec2 TransformPoint(const glm::vec2& point) const {
        glm::vec3 result = matrix * glm::vec3(point.x, point.y, 1.0f);
        return glm::vec2(result.x, result.y);
    }

    // 缁勫悎涓や釜鍙樻崲
    TransformData Combine(const TransformData& other) const {
        TransformData result;
        result.matrix = this->matrix * other.matrix;

        // 浠庣煩闃典腑鎻愬彇浣嶇疆
        result.position = glm::vec2(result.matrix[2][0], result.matrix[2][1]);

        // 浠庣煩闃典腑鎻愬彇缂╂斁
        result.scale.x = glm::length(glm::vec2(result.matrix[0][0], result.matrix[0][1]));
        result.scale.y = glm::length(glm::vec2(result.matrix[1][0], result.matrix[1][1]));

        // 浠庣煩闃典腑鎻愬彇鏃嬭浆
        if (result.scale.x > 0 && result.scale.y > 0) {
            float scaleX = result.scale.x;
            float scaleY = result.scale.y;
            result.rotation = atan2(result.matrix[0][1] / scaleY,
                result.matrix[0][0] / scaleX);
        }

        return result;
    }

    // 鑾峰彇浣嶇疆涓篤ector
    Vector GetPositionVec() const {
        return Vector(position.x, position.y);
    }

    // 璁剧疆浣嶇疆
    void SetPosition(const Vector& pos) {
        position = glm::vec2(pos.x, pos.y);
        UpdateMatrix();
    }

    void SetPosition(float x, float y) {
        position = glm::vec2(x, y);
        UpdateMatrix();
    }

    // 璁剧疆缂╂斁
    void SetScale(const Vector& scl) {
        scale = glm::vec2(scl.x, scl.y);
        UpdateMatrix();
    }

    void SetScale(float sx, float sy) {
        scale = glm::vec2(sx, sy);
        UpdateMatrix();
    }

    void SetScale(float s) {
        scale = glm::vec2(s, s);
        UpdateMatrix();
    }

    // 璁剧疆鏃嬭浆锛堝姬搴︼級
    void SetRotation(float rad) {
        rotation = rad;
        UpdateMatrix();
    }

    // 璁剧疆鏃嬭浆锛堣搴︼級
    void SetRotationDegrees(float deg) {
        rotation = glm::radians(deg);
        UpdateMatrix();
    }

    // 鑾峰彇缂╂斁涓篤ector
    Vector GetScaleVec() const {
        return Vector(scale.x, scale.y);
    }
};

// 鏁堟灉鍩虹被鎺ュ彛
class IAttachmentEffect {
public:
    virtual ~IAttachmentEffect() = default;

    // 鏇存柊鏁堟灉
    virtual void Update(float deltaTime) = 0;

    // 缁樺埗鏁堟灉锛宲arentTransform涓虹埗绾у彉鎹?
    virtual void Draw(SDL_Renderer* renderer, const TransformData& parentTransform) = 0;

    // 璁剧疆浣嶇疆锛堢浉瀵逛綅缃級
    virtual void SetPosition(const Vector& position) = 0;
    virtual void SetPosition(float x, float y) = 0;

    // 璁剧疆缂╂斁
    virtual void SetScale(const Vector& scale) = 0;
    virtual void SetScale(float scale) = 0;
    virtual void SetScale(float sx, float sy) = 0;

    // 璁剧疆鏃嬭浆锛堝姬搴︼級
    virtual void SetRotation(float rotation) = 0;

    // 璁剧疆棰滆壊瑕嗙洊
    virtual void OverrideColor(const SDL_Color& color) = 0;

    // 璁剧疆閫忔槑搴?
    virtual void SetAlpha(float alpha) = 0;

    // 璁剧疆鍙鎬?
    virtual void SetVisible(bool visible) = 0;

    // 浼犳挱棰滆壊鍒版墍鏈夊瓙鏁堟灉
    virtual void PropagateColor(const SDL_Color& color,
        bool enableAdditive, const SDL_Color& additiveColor,
        bool enableOverlay, const SDL_Color& overlayColor) = 0;

    // 璁剧疆鍙樻崲鐭╅樀
    virtual void SetTransformMatrix(const glm::mat3& matrix) = 0;

    // 鑾峰彇鍙樻崲鏁版嵁
    virtual TransformData GetTransform() const = 0;

    // 妫€鏌ユ槸鍚﹀瓨娲?
    virtual bool IsAlive() const = 0;

    // 閿€姣佹晥鏋?
    virtual void Die() = 0;

    // 鍒嗙鏁堟灉锛堜粠闄勪欢涓Щ闄や絾涓嶉攢姣侊級
    virtual void Detach() = 0;

    // 妫€鏌ユ槸鍚﹁闄勫姞
    virtual bool IsAttached() const = 0;
    virtual void SetAttached(bool attached) = 0;

    // 鑾峰彇鏁堟灉绫诲瀷
    virtual EffectType GetEffectType() const = 0;

    // 鑾峰彇鏁堟灉鍚嶇О锛堢敤浜庤皟璇曪級
    virtual std::string GetEffectName() const = 0;
};

// 鍗曚釜闄勪欢鏁堟灉
struct AttachmentEffect {
    std::shared_ptr<IAttachmentEffect> effect;
    TransformData offsetTransform;           // 鐩稿浜庣埗绾х殑鍋忕Щ鍙樻崲
    bool dontDrawIfParentHidden = false;    // 鐖剁骇闅愯棌鏃朵笉缁樺埗
    bool dontPropagateColor = false;        // 涓嶄紶鎾鑹?
    bool inheritTransform = true;           // 缁ф壙鐖剁骇鍙樻崲
    bool inheritVisibility = true;          // 缁ф壙鐖剁骇鍙鎬?

    AttachmentEffect() = default;

    AttachmentEffect(std::shared_ptr<IAttachmentEffect> eff,
        const TransformData& offset = TransformData())
        : effect(std::move(eff)), offsetTransform(offset) {
    }

    bool IsValid() const { return effect != nullptr && effect->IsAlive(); }

    // 鑾峰彇鏈€缁堝彉鎹紙鐖剁骇鍙樻崲 + 鍋忕Щ锛?
    TransformData GetFinalTransform(const TransformData& parentTransform) const {
        if (!inheritTransform) {
            return offsetTransform;
        }
        return parentTransform.Combine(offsetTransform);
    }
};

// 绠＄悊澶氫釜鏁堟灉
class Attachment {
private:
    std::vector<AttachmentEffect> mEffects;
    TransformData mTransform;
    bool mVisible = true;
    bool mAlive = true;
    bool mAttached = false;
    std::string mName;

    // 棰滆壊鐩稿叧
    SDL_Color mColorOverride = { 255, 255, 255, 255 };
    bool mHasColorOverride = false;
    float mAlpha = 1.0f;

public:
    static constexpr int MAX_EFFECTS_PER_ATTACHMENT = 32;

    Attachment(const std::string& name = "");
    ~Attachment();

    // 娣诲姞鏁堟灉
    bool AddEffect(std::shared_ptr<IAttachmentEffect> effect,
        const Vector& offset = Vector::zero(),
        const Vector& scale = Vector::one(),
        float rotation = 0.0f);

    bool AddEffect(std::shared_ptr<IAttachmentEffect> effect,
        float offsetX = 0.0f, float offsetY = 0.0f,
        float scaleX = 1.0f, float scaleY = 1.0f,
        float rotation = 0.0f);

    // 绉婚櫎鏁堟灉
    bool RemoveEffect(int index);
    bool RemoveEffect(std::shared_ptr<IAttachmentEffect> effect);
    void RemoveAllEffects();

    // 鏌ユ壘鏁堟灉
    std::shared_ptr<IAttachmentEffect> FindEffect(int index) const;

    // 鏌ユ壘鎸囧畾绫诲瀷鐨勭涓€涓晥鏋?
    template<typename T>
    std::shared_ptr<T> FindFirstEffectOfType(EffectType type) const {
        for (const auto& effectInfo : mEffects) {
            if (effectInfo.effect && effectInfo.effect->GetEffectType() == type) {
                return std::dynamic_pointer_cast<T>(effectInfo.effect);
            }
        }
        return nullptr;
    }

    // 鏌ユ壘鎵€鏈夋寚瀹氱被鍨嬬殑鐗规晥
    template<typename T>
    std::vector<std::shared_ptr<T>> FindAllEffectsOfType(EffectType type) const {
        std::vector<std::shared_ptr<T>> results;
        for (const auto& effectInfo : mEffects) {
            if (effectInfo.effect && effectInfo.effect->GetEffectType() == type) {
                if (auto casted = std::dynamic_pointer_cast<T>(effectInfo.effect)) {
                    results.push_back(casted);
                }
            }
        }
        return results;
    }

    void Update(float deltaTime);

    void Draw(SDL_Renderer* renderer, const TransformData& parentTransform = TransformData());

    void SetPosition(const Vector& position);
    void SetPosition(float x, float y);

    // 璁剧疆缂╂斁
    void SetScale(const Vector& scale);
    void SetScale(float scale);
    void SetScale(float sx, float sy);

    // 璁剧疆鏃嬭浆
    void SetRotation(float rotation);

    // 璁剧疆棰滆壊瑕嗙洊
    void OverrideColor(const SDL_Color& color);

    // 璁剧疆閫忔槑搴?
    void SetAlpha(float alpha);

    // 璁剧疆鍙鎬?
    void SetVisible(bool visible);

    // 浼犳挱棰滆壊鍒版墍鏈夋晥鏋?
    void PropagateColor(const SDL_Color& color,
        bool enableAdditive, const SDL_Color& additiveColor,
        bool enableOverlay, const SDL_Color& overlayColor);

    // 璁剧疆鍙樻崲鐭╅樀
    void SetTransformMatrix(const glm::mat3& matrix);

    // 鑾峰彇鍙樻崲鏁版嵁
    TransformData GetTransform() const { return mTransform; }

    // 妫€鏌ユ槸鍚﹀瓨娲?
    bool IsAlive() const { return mAlive; }

    // 閿€姣侀檮浠?
    void Die();

    // 鍒嗙鎵€鏈夋晥鏋?
    void Detach();

    // 鑾峰彇鏁堟灉鏁伴噺
    size_t GetEffectCount() const { return mEffects.size(); }

    // 妫€鏌ユ槸鍚﹀凡婊?
    bool IsFull() const { return mEffects.size() >= MAX_EFFECTS_PER_ATTACHMENT; }

    // 鑾峰彇/璁剧疆鍚嶇О
    const std::string& GetName() const { return mName; }
    void SetName(const std::string& name) { mName = name; }

    // 鑾峰彇鎵€鏈夋晥鏋?
    const std::vector<AttachmentEffect>& GetEffects() const { return mEffects; }
    std::vector<AttachmentEffect>& GetEffects() { return mEffects; }

    // 娣诲姞Animator
    bool AddAnimator(std::shared_ptr<Animator> animator,
        const Vector& offset = Vector::zero(),
        const Vector& scale = Vector::one(),
        float rotation = 0.0f);

    bool AddAnimator(std::shared_ptr<Animator> animator,
        float offsetX = 0.0f, float offsetY = 0.0f,
        float scaleX = 1.0f, float scaleY = 1.0f,
        float rotation = 0.0f);

    // 鏌ユ壘Animator
    std::shared_ptr<Animator> FindFirstAnimator() const;
    std::vector<std::shared_ptr<Animator>> FindAllAnimators() const;

private:

    void CleanupDeadEffects();
};

// 闄勪欢绯荤粺绠＄悊鍣?
class AttachmentSystem {
private:
    std::unordered_map<std::string, std::shared_ptr<Attachment>> mNamedAttachments;
    std::vector<std::shared_ptr<Attachment>> mActiveAttachments;

public:
    AttachmentSystem() = default;
    ~AttachmentSystem();

    // 鍒涘缓鏂伴檮浠?
    std::shared_ptr<Attachment> CreateAttachment(const std::string& name = "");

    // 鑾峰彇闄勪欢
    std::shared_ptr<Attachment> GetAttachment(const std::string& name);
    bool HasAttachment(const std::string& name) const;

    // 绉婚櫎闄勪欢
    bool RemoveAttachment(const std::string& name);
    void RemoveAllAttachments();

    // 鏇存柊鎵€鏈夐檮浠?
    void UpdateAll(float deltaTime);

    // 缁樺埗鎵€鏈夐檮浠?
    void DrawAll(SDL_Renderer* renderer);

    // 鑾峰彇闄勪欢鏁伴噺
    size_t GetAttachmentCount() const { return mActiveAttachments.size(); }

    // 娓呯悊宸查攢姣佺殑闄勪欢
    void CleanupDeadAttachments();

    // 鑾峰彇鎵€鏈夐檮浠跺悕绉?
    std::vector<std::string> GetAllAttachmentNames() const;

    // 閬嶅巻鎵€鏈夐檮浠?
    template<typename Func>
    void ForEachAttachment(Func func) {
        for (auto& attachment : mActiveAttachments) {
            if (attachment && attachment->IsAlive()) {
                func(attachment);
            }
        }
    }

private:
    void UpdateAttachment(std::shared_ptr<Attachment>& attachment, float deltaTime);
};

#endif 