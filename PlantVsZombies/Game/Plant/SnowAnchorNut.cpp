#include "SnowAnchorNut.h"

#include "Game/Board/Board.h"

namespace {
	constexpr int kSnowAnchorNutHealth = 3000; // 雪锚果基础生命值；存活期间可持续承担冻土冲击
	constexpr float kGroundCrackDownstreamMultiplier = 1.0f / 3.0f; // 拦住地裂后左侧植物承受的伤害倍率上限
}

void SnowAnchorNut::SetupPlant()
{
	WallNut::SetupPlant();
	mPlantHealth = kSnowAnchorNutHealth;
	mPlantMaxHealth = kSnowAnchorNutHealth;
	mLastBraceReady = IsWinterGroundAnchorReady();
	InvalidateDamageTexture();
	UpdateTexture(false);
}

void SnowAnchorNut::PlantUpdate()
{
	WallNut::PlantUpdate();
	const bool braceReady = IsWinterGroundAnchorReady();
	if (braceReady != mLastBraceReady) {
		mLastBraceReady = braceReady;
		RefreshBracePresentation();
	}
}

bool SnowAnchorNut::IsWinterGroundAnchorReady() const
{
	return IsActive() && !IsSquished() && mPlantHealth > 0 && mBoard
		&& mBoard->IsCellFrozen(mRow, mColumn);
}

WinterGroundImpactResponse SnowAnchorNut::ResolveWinterGroundImpact(
	WinterGroundImpactKind kind)
{
	if (!IsWinterGroundAnchorReady()) return {};

	WinterGroundImpactResponse response;
	response.intercepted = true;
	if (kind == WinterGroundImpactKind::COLLISION) {
		response.containsScatter = true;
	}
	else {
		response.downstreamDamageMultiplierCap = kGroundCrackDownstreamMultiplier;
	}
	return response;
}

const std::string& SnowAnchorNut::GetBodyTextureKey() const
{
	return IsWinterGroundAnchorReady()
		? ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_BRACED_BODY
		: ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_BODY;
}

const std::string& SnowAnchorNut::GetCrackedTextureKey(int damageStage) const
{
	if (IsWinterGroundAnchorReady()) {
		return damageStage >= 2
			? ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_BRACED_CRACKED2
			: ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_BRACED_CRACKED1;
	}
	return damageStage >= 2
		? ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_CRACKED2
		: ResourceKeys::Textures::IMAGE_SNOWANCHORNUT_CRACKED1;
}

void SnowAnchorNut::RefreshBracePresentation()
{
	InvalidateDamageTexture();
	UpdateTexture(false);
}
