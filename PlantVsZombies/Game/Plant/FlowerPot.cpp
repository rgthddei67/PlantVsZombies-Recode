#include "FlowerPot.h"

#include "../Board.h"
#include "../ShadowComponent.h"

#include <algorithm>

namespace {
	constexpr float kFlowerPotShadowOffsetX = 2.0f;	// C# DrawShadow 的 -3px 通用落点再左移 1px，单位：px
	constexpr float kFlowerPotShadowOffsetY = 46.0f;	// C# DrawShadow 的 51px 通用落点再上移 5px，单位：px
}

void FlowerPot::SetupPlant()
{
	Plant::SetupPlant();
	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(kFlowerPotShadowOffsetX, kFlowerPotShadowOffsetY));
		// C# TodDrawImageCenterScaledF 使用 1:1 比例；通用组件默认的纵向 0.75 压缩会让阴影几乎全被盆底遮住。
		shadow->SetScale(Vector(1.0f, 1.0f));
	}
}

void FlowerPot::Draw(Graphics* g)
{
	AnimatedObject::Draw(g);
}

void FlowerPot::PlantUpdate()
{
	// 原版 100cs 保护只按游戏时间递减，不受雨势植物行动倍率影响。
	if (mBiteProtectionTimer > 0.0f) {
		mBiteProtectionTimer = std::max(0.0f,
			mBiteProtectionTimer - DeltaTime::GetDeltaTime());
	}

	const bool coveredNow = mBoard && mBoard->GetTopPlantAt(mRow, mColumn) != this;
	if (coveredNow != mCovered) {
		// 原版在上层植物落下时把花盆 anim rate 设为 0，露出后恢复原待机动画。
		mCovered = coveredNow;
		if (PausesAnimationWhenCovered()) {
			if (mCovered) PauseAnimation();
			else PlayAnimation();
		}
	}
}

bool FlowerPot::CanBeEaten() const
{
	return Plant::CanBeEaten() && mBiteProtectionTimer <= 0.0f;
}

void FlowerPot::SaveExtraData(nlohmann::json& j) const
{
	j["biteProtectionTimer"] = mBiteProtectionTimer;
}

void FlowerPot::LoadExtraData(const nlohmann::json& j)
{
	// 旧档没有该字段时视为保护已结束，避免读档凭空重新获得一秒保护。
	mBiteProtectionTimer = std::max(0.0f,
		j.value("biteProtectionTimer", 0.0f));
	mCovered = false;
}
