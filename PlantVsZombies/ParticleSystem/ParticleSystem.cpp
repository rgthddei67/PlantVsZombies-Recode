#include "../ParticleSystem/ParticleSystem.h"

ParticleSystem::ParticleSystem(SDL_Renderer* Renderer)
    : renderer(Renderer) {
}

ParticleSystem::~ParticleSystem() {
    ClearAll();
}

void ParticleSystem::UpdateAll()
{
    CleanupInactiveEmitters();
    for (auto& emitter : emitters) {
        emitter->Update();
    }
}

void ParticleSystem::DrawAll() {
    for (auto& emitter : emitters) {
        emitter->Draw(renderer);
    }
}

void ParticleSystem::ClearAll() {
    emitters.clear();
}

void ParticleSystem::EmitEffect(ParticleType type, const SDL_FPoint& position, int count) {
    auto emitter = std::make_unique<ParticleEmitter>(renderer);
    emitter->Initialize(type, position);
    emitter->SetOneShot(true);
    emitter->SetAutoDestroyTime(-2);
    emitter->EmitParticles(count);
    emitters.push_back(std::move(emitter));
}

void ParticleSystem::EmitEffect(ParticleType type, float x, float y, int count) {
    EmitEffect(type, SDL_FPoint{ x, y }, count);
}

void ParticleSystem::EmitEffect(ParticleType type, const Vector& position, int count) {
    EmitEffect(type, static_cast<SDL_FPoint>(position), count);
}

// 循环的特效
ParticleEmitter* ParticleSystem::CreatePersistentEmitter(ParticleType type, const SDL_FPoint& position) {
    auto emitter = std::make_unique<ParticleEmitter>(renderer);
    emitter->Initialize(type, position);
    emitter->SetSpawnRate(10);  // 10帧发射一次
    ParticleEmitter* ptr = emitter.get();
    emitters.push_back(std::move(emitter));
    return ptr;
}

void ParticleSystem::RemoveEmitter(ParticleEmitter* emitter) {
    if (emitter) {
        emitter->Stop();
    }
}

void ParticleSystem::CleanupInactiveEmitters() {
    emitters.erase(
        std::remove_if(emitters.begin(), emitters.end(),
            [](const std::unique_ptr<ParticleEmitter>& emitter) {
                return emitter->ShouldDestroy();
            }),
        emitters.end()
    );
}

int ParticleSystem::GetTotalParticles() const {
    int total = 0;
    for (const auto& emitter : emitters) {
        total += emitter->GetActiveParticleCount();
    }
    return total;
}

size_t ParticleSystem::GetActiveEmitters() const {
    return emitters.size();
}