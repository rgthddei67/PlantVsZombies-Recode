#include "ParticleEffect.h"
#include "../DeltaTime.h"

ParticleEffect::ParticleEffect()
    : systemTimer(0.0f)
    , systemDuration(-1.0f)
    , active(false) {
}

void ParticleEffect::InitializeFromConfig(const ParticleEffectConfig& config, Graphics* graphics, const Vector& pos) {
    position = pos;
    systemTimer = 0.0f;
    active = true;

    // 为每个发射器配置创建发射器
    for (const EmitterConfig& emitterConfig : config.emitters) {
        auto emitter = std::make_unique<ParticleEmitter>(graphics);

        // 计算发射器位置（基础位置 + 偏移）
        Vector emitterPos = position;
        emitterPos.x += emitterConfig.emitterOffsetX;
        emitterPos.y += emitterConfig.emitterOffsetY;

        // 从XML配置初始化发射器
        emitter->Initialize(emitterConfig, emitterPos);

        // 设置系统持续时间（使用第一个发射器的系统持续时间）
        if (emitters.empty() && emitterConfig.systemDuration > 0.0f) {
            systemDuration = emitterConfig.systemDuration;
        }

        emitters.push_back(std::move(emitter));
    }
}

void ParticleEffect::Update() {
    if (!active) return;

    float deltaTime = DeltaTime::GetDeltaTime();
    systemTimer += deltaTime;

    // 检查系统持续时间
    if (systemDuration > 0.0f && systemTimer >= systemDuration) {
        active = false;
        // 停止所有发射器
        for (auto& emitter : emitters) {
            emitter->Stop();
        }
    }

    // 更新所有发射器
    for (auto& emitter : emitters) {
        emitter->Update();
    }
}

void ParticleEffect::Draw() {
    for (auto& emitter : emitters) {
        emitter->Draw();
    }
}

bool ParticleEffect::ShouldDestroy() const {
    if (!active) {
        // 检查所有发射器是否都应该销毁
        for (const auto& emitter : emitters) {
            if (!emitter->ShouldDestroy()) {
                return false;
            }
        }
        return true;
    }
    return false;
}

void ParticleEffect::SetPosition(const Vector& pos) {
    Vector offset = pos - position;
    position = pos;

    // 更新所有发射器的位置
    for (auto& emitter : emitters) {
        Vector emitterPos = emitter->GetPosition();
        emitterPos.x += offset.x;
        emitterPos.y += offset.y;
        emitter->SetPosition(emitterPos);
    }
}

void ParticleEffect::ApplySystemFields(const std::vector<ParticleField>& systemFields) {
    // 系统场效果应用于发射器位置
    // 这个功能将在发射器中实现
}
