#pragma once
#ifndef __PARTICLE_EMITTER_H__
#define __PARTICLE_EMITTER_H__

#include "Particle.h"
#include "ParticleXMLConfig.h"
#include "../Graphics.h"
#include <vector>

class ParticleEmitter {
private:
	Graphics* m_graphics;
	std::vector<Particle> particles;

	Vector position;
	bool active;
	float spawnTimer;
	int spawnRate;
	int maxParticles;

	bool isOneShot;
	int particlesToEmit;
	int particlesEmitted;

	EmitterConfig xmlConfig;
	std::vector<ParticleField> activeFields;
	float systemTimer;

public:
	ParticleEmitter(Graphics* g = nullptr);
	~ParticleEmitter() = default;

	void SetGraphics(Graphics* g) { m_graphics = g; }

	void Initialize(const EmitterConfig& config, const Vector& pos);

	void SetSpawnRate(int rate) { spawnRate = rate; }
	void SetMaxParticles(int max) { maxParticles = max; }
	void SetOneShot(bool oneShot) { isOneShot = oneShot; }

	void EmitParticles(int count);
	void Stop() { active = false; }
	void Clear();

	bool IsActive() const { return active; }
	bool ShouldDestroy() const;
	int GetActiveParticleCount() const;
	void SetPosition(const Vector& pos) { position = pos; }
	Vector GetPosition() const { return position; }

	void Update();
	void Draw();

private:
	void EmitSingleParticle();
	Particle* GetFreeParticle();
	Vector GetSpawnPosition() const;
};

#endif