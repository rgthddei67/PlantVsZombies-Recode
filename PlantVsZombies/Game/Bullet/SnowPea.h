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
		// C# 在扣血前进入 ApplyChill；先缓存资格，避免致死寒冰弹因随后进入垂死态而漏掉正常音效。
		const bool canBeChilled = zombie->CanBeChilled();
		Bullet::BulletHitZombie(zombie);

		// 原版 ApplyChill 先过 CanBeChilled 总闸；免疫目标既不上状态，也不播放减速音效。
		if (canBeChilled) {
			if (zombie->GetCooldownTimer() <= 0.0f
				&& zombie->mHelmType == HelmType::HELMTYPE_NONE
				&& zombie->mShieldType == ShieldType::SHIELDTYPE_NONE) {
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_COOLDOWNZOMBIE, 0.22f);
			}
			zombie->SetCooldown(7.5f);
		}

		g_particleSystem->EmitEffect("SnowPeaBulletHit", GetPosition());

		if (zombie->mHelmType == HelmType::HELMTYPE_TRAFFIC_CONE ||
			zombie->mHelmType == HelmType::HELMTYPE_FOOTBALL) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE, 0.2f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_HITCONE2, 0.2f);
		}
		else if (zombie->mHelmType == HelmType::HELMTYPE_BUCKET ||
			zombie->mShieldType == ShieldType::SHIELDTYPE_DOOR ||
			zombie->mShieldType == ShieldType::SHIELDTYPE_LADDER ||
			zombie->mZombieType == ZombieType::ZOMBIE_ZAMBONI ||
			zombie->mZombieType == ZombieType::ZOMBIE_GILDED_ZAMBONI) {
			int random = GameRandom::Range(1, 2);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT, 0.2f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT2, 0.2f);
		}
		else {
			int random = GameRandom::Range(1, 3);
			if (random == 1)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY1, 0.2f);
			else if (random == 2)
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY2, 0.2f);
			else
				AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PEABULLET_HIT_BODY3, 0.2f);
		}
	}
};

#endif
