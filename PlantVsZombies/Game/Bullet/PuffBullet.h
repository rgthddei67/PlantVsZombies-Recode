#pragma once
#ifndef _BULLET_PUFF_H
#define _BULLET_PUFF_H
#include "Bullet.h"

class PuffBullet : public Bullet
{
public:
	using Bullet::Bullet;

	void Start() override {
		GameObject::Start();
		this->mTexture = ResourceManager::GetInstance().GetTexture(
			"IMAGE_PUFFSHROOM_PUFF1");
		this->mScale = 0.68f;
	}

protected:
	void BulletHitZombie(Zombie* zombie) override
	{
		Bullet::BulletHitZombie(zombie);
		g_particleSystem->EmitEffect("PuffShroomHit", GetPosition());
		if (zombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE ||
			zombie->mHelmType == HelmType::HELMTYPE_FOOTBALL) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE, 0.3f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE2, 0.3f);
		}
		else if (zombie->mHelmType == HelmType::HELMTYPE_BUCKET ||
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

#endif