#pragma once
#ifndef _SUN_H
#define _SUN_H
#include "Coin.h"

class Sun : public Coin {
private:
	int SunPoint = 25;	// 收集后增加的阳光点数

public:
	Sun(Board* board, const Vector& position, float scale = 0.75f,
		const std::string& tag = "Sun", bool autoDestroy = true)
		: Coin(board, AnimationType::ANIM_SUN, position,
			Vector(65, 65), scale, tag, autoDestroy)
	{
		speedFast = 700.0f;
		speedSlow = 500.0f;
		slowDownDistance = 130.0f;
	}

	void OnReachTargetBack() override
	{
		Coin::OnReachTargetBack();
		mBoard->AddSun(SunPoint);
	}

    void SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent) override
    {
        if (clickComponent == nullptr) return;
        clickComponent->onClick = [this]() {
			AudioSystem::PlaySound(AudioConstants::SOUND_COLLECT_SUN, 0.5f);
			StartMoveToTarget(Vector(10, 10), speedFast, speedSlow, slowDownDistance);
        };
    }

};
#endif