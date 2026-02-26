#pragma once
#include "Bullet.h"

class PeaBullet : public Bullet
{
public:
	using Bullet::Bullet;

protected:
	void BulletHitZombie(std::shared_ptr<Zombie> zombie) override
	{
		Bullet::BulletHitZombie(zombie);
		g_particleSystem->EmitEffect(ParticleType::PEA_BULLET_HIT, GetPosition());	
		if (zombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE || 
			zombie->mHelmType == HelmType::HELMTYPE_FOOTBALL) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE, 0.3f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE2, 0.3f);
		}
		else if (zombie->mHelmType == HelmType::HELMTYPE_PAIL || 
			zombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
			zombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
			zombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT, 0.3f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT2, 0.3f);
		}
		else {
			int random = GameRandom::Range(1, 3);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.3f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY2, 0.3f);
			else
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY3, 0.3f);
		}
	}
};