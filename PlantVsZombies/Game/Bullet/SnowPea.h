#pragma once
#ifndef _SNOWPEA_H
#define _SNOWPEA_H
#include "Bullet.h"

class SnowPeaBullet : public Bullet
{
public:
	using Bullet::Bullet;

	void Start() override {
		GameObject::Start();
		this->mTexture = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_PROJECTILESNOWPEA);
	}

protected:
	void BulletHitZombie(Zombie* zombie) override
	{
		Bullet::BulletHitZombie(zombie);

		if (zombie->GetCooldownTimer() <= 0.0f && zombie->mHelmType == HelmType::HELMTYPE_NONE
			&& zombie->mShieldType == ShieldType::SHIELDTYPE_NONE) {
			AudioSystem::PlaySound("SOUND_COOLDOWNZOMBIE", 0.22f);
		}

		zombie->SetCooldown(7.5f);
		g_particleSystem->EmitEffect("SnowPeaBulletHit", GetPosition());

		if (zombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE ||
			zombie->mHelmType == HelmType::HELMTYPE_FOOTBALL) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE, 0.15f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE2, 0.15f);
		}
		else if (zombie->mHelmType == HelmType::HELMTYPE_BUCKET ||
			zombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
			zombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
			zombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT, 0.15f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT2, 0.15f);
		}
		else {
			int random = GameRandom::Range(1, 3);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.19f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY2, 0.19f);
			else
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY3, 0.19f);
		}
	}
};

#endif