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

// 效果类型
enum class EffectType {
    EFFECT_NONE = 0,
    EFFECT_ANIMATION     // 动画效果
};

// 变换信息结构
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

    // 更新变换矩阵
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

    // 变换一个点
    glm::vec2 TransformPoint(const glm::vec2& point) const {
        glm::vec3 result = matrix * glm::vec3(point.x, point.y, 1.0f);
        return glm::vec2(result.x, result.y);
    }

    // 组合两个变换
    TransformData Combine(const TransformData& other) const {
        TransformData result;
        result.matrix = this->matrix * other.matrix;

        // 从矩阵中提取位置
        result.position = glm::vec2(result.matrix[2][0], result.matrix[2][1]);

        // 从矩阵中提取缩放
        result.scale.x = glm::length(glm::vec2(result.matrix[0][0], result.matrix[0][1]));
        result.scale.y = glm::length(glm::vec2(result.matrix[1][0], result.matrix[1][1]));

        // 从矩阵中提取旋转
        if (result.scale.x > 0 && result.scale.y > 0) {
            float scaleX = result.scale.x;
            float scaleY = result.scale.y;
            result.rotation = atan2(result.matrix[0][1] / scaleY,
                result.matrix[0][0] / scaleX);
        }

        return result;
    }

    // 获取位置为Vector
    Vector GetPositionVec() const {
        return Vector(position.x, position.y);
    }

    // 设置位置
    void SetPosition(const Vector& pos) {
        position = glm::vec2(pos.x, pos.y);
        UpdateMatrix();
    }

    void SetPosition(float x, float y) {
        position = glm::vec2(x, y);
        UpdateMatrix();
    }

    // 设置缩放
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

    // 设置旋转（弧度）
    void SetRotation(float rad) {
        rotation = rad;
        UpdateMatrix();
    }

    // 设置旋转（角度）
    void SetRotationDegrees(float deg) {
        rotation = glm::radians(deg);
        UpdateMatrix();
    }

    // 获取缩放为Vector
    Vector GetScaleVec() const {
        return Vector(scale.x, scale.y);
    }
};

// 效果基类接口
class IAttachmentEffect {
public:
    virtual ~IAttachmentEffect() = default;

    // 更新效果
    virtual void Update(float deltaTime) = 0;

    // 绘制效果，parentTransform为父级变换
    virtual void Draw(SDL_Renderer* renderer, const TransformData& parentTransform) = 0;

    // 设置位置（相对位置）
    virtual void SetPosition(const Vector& position) = 0;
    virtual void SetPosition(float x, float y) = 0;

    // 设置缩放
    virtual void SetScale(const Vector& scale) = 0;
    virtual void SetScale(float scale) = 0;
    virtual void SetScale(float sx, float sy) = 0;

    // 设置旋转（弧度）
    virtual void SetRotation(float rotation) = 0;

    // 设置颜色覆盖
    virtual void OverrideColor(const SDL_Color& color) = 0;

    // 设置透明度
    virtual void SetAlpha(float alpha) = 0;

    // 设置可见性
    virtual void SetVisible(bool visible) = 0;

    // 传播颜色到所有子效果
    virtual void PropagateColor(const SDL_Color& color,
        bool enableAdditive, const SDL_Color& additiveColor,
        bool enableOverlay, const SDL_Color& overlayColor) = 0;

    // 设置变换矩阵
    virtual void SetTransformMatrix(const glm::mat3& matrix) = 0;

    // 获取变换数据
    virtual TransformData GetTransform() const = 0;

    // 检查是否存活
    virtual bool IsAlive() const = 0;

    // 销毁效果
    virtual void Die() = 0;

    // 分离效果（从附件中移除但不销毁）
    virtual void Detach() = 0;

    // 检查是否被附加
    virtual bool IsAttached() const = 0;
    virtual void SetAttached(bool attached) = 0;

    // 获取效果类型
    virtual EffectType GetEffectType() const = 0;

    // 获取效果名称（用于调试）
    virtual std::string GetEffectName() const = 0;
};

// 单个附件效果
struct AttachmentEffect {
    std::shared_ptr<IAttachmentEffect> effect;
    TransformData offsetTransform;           // 相对于父级的偏移变换
    bool dontDrawIfParentHidden = false;    // 父级隐藏时不绘制
    bool dontPropagateColor = false;        // 不传播颜色
    bool inheritTransform = true;           // 继承父级变换
    bool inheritVisibility = true;          // 继承父级可见性

    AttachmentEffect() = default;

    AttachmentEffect(std::shared_ptr<IAttachmentEffect> eff,
        const TransformData& offset = TransformData())
        : effect(std::move(eff)), offsetTransform(offset) {
    }

    bool IsValid() const { return effect != nullptr && effect->IsAlive(); }

    // 获取最终变换（父级变换 + 偏移）
    TransformData GetFinalTransform(const TransformData& parentTransform) const {
        if (!inheritTransform) {
            return offsetTransform;
        }
        return parentTransform.Combine(offsetTransform);
    }
};

// 管理多个效果
class Attachment {
private:
    std::vector<AttachmentEffect> mEffects;
    TransformData mTransform;
    bool mVisible = true;
    bool mAlive = true;
    bool mAttached = false;
    std::string mName;

    // 颜色相关
    SDL_Color mColorOverride = { 255, 255, 255, 255 };
    bool mHasColorOverride = false;
    float mAlpha = 1.0f;

public:
    static constexpr int MAX_EFFECTS_PER_ATTACHMENT = 32;

    Attachment(const std::string& name = "");
    ~Attachment();

    // 添加效果
    bool AddEffect(std::shared_ptr<IAttachmentEffect> effect,
        const Vector& offset = Vector::zero(),
        const Vector& scale = Vector::one(),
        float rotation = 0.0f);

    bool AddEffect(std::shared_ptr<IAttachmentEffect> effect,
        float offsetX = 0.0f, float offsetY = 0.0f,
        float scaleX = 1.0f, float scaleY = 1.0f,
        float rotation = 0.0f);

    // 移除效果
    bool RemoveEffect(int index);
    bool RemoveEffect(std::shared_ptr<IAttachmentEffect> effect);
    void RemoveAllEffects();

    // 查找效果
    std::shared_ptr<IAttachmentEffect> FindEffect(int index) const;

    // 查找指定类型的第一个效果
    template<typename T>
    std::shared_ptr<T> FindFirstEffectOfType(EffectType type) const {
        for (const auto& effectInfo : mEffects) {
            if (effectInfo.effect && effectInfo.effect->GetEffectType() == type) {
                return std::dynamic_pointer_cast<T>(effectInfo.effect);
            }
        }
        return nullptr;
    }

    // 查找所有指定类型的特效
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

    // 设置缩放
    void SetScale(const Vector& scale);
    void SetScale(float scale);
    void SetScale(float sx, float sy);

    // 设置旋转
    void SetRotation(float rotation);

    // 设置颜色覆盖
    void OverrideColor(const SDL_Color& color);

    // 设置透明度
    void SetAlpha(float alpha);

    // 设置可见性
    void SetVisible(bool visible);

    // 传播颜色到所有效果
    void PropagateColor(const SDL_Color& color,
        bool enableAdditive, const SDL_Color& additiveColor,
        bool enableOverlay, const SDL_Color& overlayColor);

    // 设置变换矩阵
    void SetTransformMatrix(const glm::mat3& matrix);

    // 获取变换数据
    TransformData GetTransform() const { return mTransform; }

    // 检查是否存活
    bool IsAlive() const { return mAlive; }

    // 销毁附件
    void Die();

    // 分离所有效果
    void Detach();

    // 获取效果数量
    size_t GetEffectCount() const { return mEffects.size(); }

    // 检查是否已满
    bool IsFull() const { return mEffects.size() >= MAX_EFFECTS_PER_ATTACHMENT; }

    // 获取/设置名称
    const std::string& GetName() const { return mName; }
    void SetName(const std::string& name) { mName = name; }

    // 获取所有效果
    const std::vector<AttachmentEffect>& GetEffects() const { return mEffects; }
    std::vector<AttachmentEffect>& GetEffects() { return mEffects; }

    // 添加Animator
    bool AddAnimator(std::shared_ptr<Animator> animator,
        const Vector& offset = Vector::zero(),
        const Vector& scale = Vector::one(),
        float rotation = 0.0f);

    bool AddAnimator(std::shared_ptr<Animator> animator,
        float offsetX = 0.0f, float offsetY = 0.0f,
        float scaleX = 1.0f, float scaleY = 1.0f,
        float rotation = 0.0f);

    // 查找Animator
    std::shared_ptr<Animator> FindFirstAnimator() const;
    std::vector<std::shared_ptr<Animator>> FindAllAnimators() const;

private:

    void CleanupDeadEffects();
};

// 附件系统管理器
class AttachmentSystem {
private:
    std::unordered_map<std::string, std::shared_ptr<Attachment>> mNamedAttachments;
    std::vector<std::shared_ptr<Attachment>> mActiveAttachments;

public:
    AttachmentSystem() = default;
    ~AttachmentSystem();

    // 创建新附件
    std::shared_ptr<Attachment> CreateAttachment(const std::string& name = "");

    // 获取附件
    std::shared_ptr<Attachment> GetAttachment(const std::string& name);
    bool HasAttachment(const std::string& name) const;

    // 移除附件
    bool RemoveAttachment(const std::string& name);
    void RemoveAllAttachments();

    // 更新所有附件
    void UpdateAll(float deltaTime);

    // 绘制所有附件
    void DrawAll(SDL_Renderer* renderer);

    // 获取附件数量
    size_t GetAttachmentCount() const { return mActiveAttachments.size(); }

    // 清理已销毁的附件
    void CleanupDeadAttachments();

    // 获取所有附件名称
    std::vector<std::string> GetAllAttachmentNames() const;

    // 遍历所有附件
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