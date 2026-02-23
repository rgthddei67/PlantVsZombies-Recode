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
		// TODO:根据防具类型决定打击音效
		int random = GameRandom::Range(1, 3);
		if (random == 1) 
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.3f);
		else if (random == 2)
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY2, 0.3f);
		else
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY3, 0.3f);
	}
};