#include "SnowAnchorNut.h"

#include "../Board.h"

namespace {
	constexpr int kSnowAnchorNutHealth = 3000; // 雪锚果基础生命值；低于普通坚果，价值集中在一次锚定
	constexpr float kGroundCrackDownstreamMultiplier = 1.0f / 3.0f; // 拦住地裂后左侧植物承受的伤害倍率
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
	return IsActive() && !IsSquished() && !mBraceSpent && mBoard
		&& mBoard->IsCellFrozen(mRow, mColumn);
}

WinterGroundImpactResponse SnowAnchorNut::ResolveWinterGroundImpact(
	WinterGroundImpactKind kind)
{
	if (!IsWinterGroundAnchorReady()) return {};

	mBraceSpent = true;
	mLastBraceReady = false;
	RefreshBracePresentation();

	WinterGroundImpactResponse response;
	response.intercepted = true;
	if (kind == WinterGroundImpactKind::COLLISION) {
		response.containsScatter = true;
	}
	else {
		response.downstreamDamageMultiplier = kGroundCrackDownstreamMultiplier;
	}
	return response;
}

void SnowAnchorNut::SaveExtraData(nlohmann::json& j) const
{
	WallNut::SaveExtraData(j);
	j["winterBraceSpent"] = mBraceSpent;
}

void SnowAnchorNut::LoadExtraData(const nlohmann::json& j)
{
	mBraceSpent = j.value("winterBraceSpent", false);
	mLastBraceReady = IsWinterGroundAnchorReady();
	InvalidateDamageTexture();
	WallNut::LoadExtraData(j);
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
