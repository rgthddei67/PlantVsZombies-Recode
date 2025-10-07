#pragma once
#ifndef _SUN_H
#define _SUN_H
#include "Coin.h"

class Sun : public Coin {
private:
	int SunPoint = 25;	// 手机后增加的阳光点数

public:
	Sun(const Vector& position, float scale = 0.75f,
		const std::string& tag = "Sun", bool autoDestroy = true)
		: Coin(AnimationType::ANIM_SUN, position,
			Vector(65, 65), scale, tag, autoDestroy)
	{
	}

	void OnReachTargetBack() override
	{
		Coin::OnReachTargetBack();
	}

    void SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent) override
    {
        if (clickComponent == nullptr) return;
        clickComponent->onClick = [this]() {
			AudioSystem::PlaySound(AudioConstants::SOUND_COLLECT_SUN, 0.5f);
            StartMoveToTarget();
        };
    }

};
#endif