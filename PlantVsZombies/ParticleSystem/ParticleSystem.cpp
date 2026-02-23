#include "../ParticleSystem/ParticleSystem.h"

std::unique_ptr<ParticleSystem> g_particleSystem = nullptr;

ParticleSystem::ParticleSystem(SDL_Renderer* Renderer)
    : renderer(Renderer) {
}

ParticleSystem::~ParticleSystem() {
    ClearAll();
}

void ParticleSystem::UpdateAll()
{
    CleanupInactiveEmitters();
    for (size_t i = 0; i < emitters.size(); i++)
    {
        auto* emitter = emitters[i].get();
        emitter->Update();
    }
}

void ParticleSystem::DrawAll() {
    for (size_t i = 0; i < emitters.size(); i++)
    {
        auto* emitter = emitters[i].get();
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
    for (size_t i = 0; i < emitters.size(); i++)
    {
        total += emitters[i].get()->GetActiveParticleCount();
    }
    return total;
}

size_t ParticleSystem::GetActiveEmitters() const {
    return emitters.size();
}