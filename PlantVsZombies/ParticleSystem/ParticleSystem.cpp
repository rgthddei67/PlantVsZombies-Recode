#include "ParticleSystem.h"

std::unique_ptr<ParticleSystem> g_particleSystem = nullptr;

ParticleSystem::ParticleSystem(Graphics* graphics)
    : m_graphics(graphics) {
}

ParticleSystem::~ParticleSystem() {
    ClearAll();
}

void ParticleSystem::UpdateAll() {
    CleanupInactiveEmitters();
    for (size_t i = 0; i < emitters.size(); i++)
    {
        emitters[i].get()->Update();
    }
}

void ParticleSystem::DrawAll() {
    for (size_t i = 0; i < emitters.size(); i++)
    {
        emitters[i].get()->Draw();
    }
}

void ParticleSystem::ClearAll() {
    emitters.clear();
}

void ParticleSystem::EmitEffect(ParticleType type, const Vector& position, int count) {
    auto emitter = std::make_unique<ParticleEmitter>(m_graphics);
    emitter->Initialize(type, position);
    emitter->SetOneShot(true);
    emitter->SetAutoDestroyTime(-2.0f);  
    emitter->EmitParticles(count);
    emitters.push_back(std::move(emitter));
}

ParticleEmitter* ParticleSystem::CreatePersistentEmitter(ParticleType type, const Vector& position) {
    auto emitter = std::make_unique<ParticleEmitter>(m_graphics);
    emitter->Initialize(type, position);
    emitter->SetSpawnRate(10);   
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