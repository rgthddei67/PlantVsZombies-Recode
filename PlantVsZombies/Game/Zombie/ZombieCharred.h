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
		mAnimator->AddFrameEvent(GetRemovalFrame(), [this]() {
			GameObjectManager::GetInstance().DestroyGameObject(this);
			});
		PlayTrackOnce("anim_crumble", "", GameRandom::Range(0.75f, 0.92f), 0.0f);
	}

protected:
	/** @brief 返回当前灰烬品种的回收帧；普通灰烬沿用原有第 42 帧。 */
	virtual int GetRemovalFrame() const { return 42; }
};

#endif
