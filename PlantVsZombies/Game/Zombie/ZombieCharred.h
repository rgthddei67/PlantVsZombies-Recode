#pragma once
#ifndef _ZOMBIE_CHERRY_H
#define _ZOMBIE_CHERRY_H

#include "../GameObjectManager.h"
#include "../AnimatedObject.h"

class ZombieCharred : public AnimatedObject {
public:
	using AnimatedObject::AnimatedObject;

	void Start() override
	{
		AnimatedObject::Start();
		mAnimator->AddFrameEvent(42, [=]() {
			GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
			});
		PlayTrackOnce("anim_crumble", "", GameRandom::Range(0.75f, 0.92f), 0.0f);
	}
};

#endif