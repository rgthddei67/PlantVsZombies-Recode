#include "Garlic.h"

#include "../ShadowComponent.h"
#include "../../ResourceKeys.h"

namespace {
	constexpr int kGarlicHealth = 400;                  // 经典大蒜基础生命值
	constexpr float kGarlicShadowOffsetY = 26.0f;       // 阴影相对逻辑中心的垂直偏移，单位：px
}

void Garlic::SetupPlant()
{
	Plant::SetupPlant();
	mPlantHealth = kGarlicHealth;
	mPlantMaxHealth = kGarlicHealth;
	if (auto* shadow = GetShadow()) {
		shadow->SetOffset(Vector(4.0f, kGarlicShadowOffsetY));
	}
	ApplyDamagePresentation();
}

void Garlic::PlantUpdate()
{
	// C# AnimateGarlic 每帧从生命派生外观；阶段未变化时本函数只做整数比较。
	ApplyDamagePresentation();
}

void Garlic::LoadExtraData(const nlohmann::json&)
{
	// 不保存重复阶段；正式生命恢复完成后只重建终态，不产生声音或其他反馈。
	mDamageStage = -1;
	ApplyDamagePresentation();
}

void Garlic::ApplyDamagePresentation()
{
	if (!mAnimator || mPlantMaxHealth <= 0) return;

	// 原版阈值是严格小于 1/3、2/3；整数边界不能沿用坚果的 <= 口径。
	const int nextStage = mPlantHealth < mPlantMaxHealth / 3
		? 2
		: (mPlantHealth < mPlantMaxHealth * 2 / 3 ? 1 : 0);
	if (nextStage == mDamageStage) return;

	const Texture* face = nullptr;
	if (nextStage == 1) {
		face = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_GARLIC_BODY2);
	}
	else if (nextStage == 2) {
		face = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_GARLIC_BODY3);
	}
	if (nextStage > 0 && !face) return;

	mDamageStage = nextStage;
	mAnimator->SetTrackImage("anim_face", face);
	const bool showStems = nextStage < 2;
	mAnimator->SetTrackVisible("Garlic_stem1", showStems);
	mAnimator->SetTrackVisible("Garlic_stem2", showStems);
	mAnimator->SetTrackVisible("Garlic_stem3", showStems);
}
