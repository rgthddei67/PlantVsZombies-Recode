#include "IceWallEngineerZombie.h"

#include "../Board.h"
#include "../IceWall.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"

#include <algorithm>

namespace {
	constexpr int kEngineerBodyHealth = 800;              // 工程师本体生命；威胁主体仍是施工而非纯耐久
	constexpr float kConstructionDuration = 4.0f;         // 从停步到冰墙原子提交的施工游戏秒数
	constexpr float kFrontierTriggerPadding = 18.0f;      // 僵尸前缘越入霜线多少像素后开始施工
	constexpr float kBuildParticleInterval = 0.45f;       // 施工碎冰反馈间隔，单位游戏秒
}

void IceWallEngineerZombie::SetupZombie()
{
	// 复用路障的完整时间轴和既有帧事件，施工仅由逻辑计时驱动。
	ConeZombie::SetupZombie();
	mBodyHealth = kEngineerBodyHealth;
	mBodyMaxHealth = kEngineerBodyHealth;
	mConstructionPhase = ConstructionPhase::MOVING;
	mConstructionRemaining = 0.0f;
	mBuildWallCenterX = 0.0f;
	mBuildParticleTimer = 0.0f;
	mConstructionUsed = false;
	ApplyEngineerEquipmentTextures();
	if (mIsPreview) PlayTrack("anim_idle");
}

void IceWallEngineerZombie::ZombieMove(float scaledDelta, Transform* transform)
{
	if (!transform) return;
	if (mConstructionPhase == ConstructionPhase::BUILDING) {
		if (ShouldAbortConstruction()) CancelConstruction(false);
		else return;
	}
	if (!mConstructionUsed && CanBeginConstruction() && mCollider) {
		const int firstFrozen = mBoard->GetFirstFrozenColumn();
		const float frontierX = mBoard->GetCellCenterPosition(mRow, firstFrozen).x
			- CELL_COLLIDER_SIZE_X * 0.5f;
		const SDL_FRect bounds = mCollider->GetBoundingBox();
		if (bounds.x <= frontierX + kFrontierTriggerPadding) {
			if (mBoard->HasIceWall()) {
				// 到达施工点时已有全场唯一墙，本工程师不在墙碎后回头补建。
				mConstructionUsed = true;
				mConstructionPhase = ConstructionPhase::COMPLETED;
			}
			else {
				BeginConstruction(frontierX);
				return;
			}
		}
	}
	ConeZombie::ZombieMove(scaledDelta, transform);
}

void IceWallEngineerZombie::ZombieUpdate(float scaledTime)
{
	if (mConstructionPhase != ConstructionPhase::BUILDING) return;
	if (ShouldAbortConstruction()) {
		CancelConstruction(false);
		return;
	}

	mConstructionRemaining = std::max(0.0f,
		mConstructionRemaining - std::max(0.0f, scaledTime));
	mBuildParticleTimer -= std::max(0.0f, scaledTime);
	if (mBuildParticleTimer <= 0.0f) {
		mBuildParticleTimer = kBuildParticleInterval;
		if (g_particleSystem && mBoard) {
			const Vector anchor(mBuildWallCenterX,
				mBoard->GetZombieCollisionY(mRow, mBuildWallCenterX) - 24.0f);
			g_particleSystem->EmitEffect("IceWallBuild", anchor);
		}
	}
	if (mConstructionRemaining <= 0.0f) CompleteConstruction();
}

bool IceWallEngineerZombie::CanBeginConstruction() const
{
	return mBoard && !mIsPreview && IsActive() && !mIsDying
		&& !IsMindControlled() && HasHead() && mBoard->SupportsWinterTemperature()
		&& mBoard->GetFrozenColumnCount() > 0;
}

bool IceWallEngineerZombie::ShouldAbortConstruction() const
{
	return !mBoard || !IsActive() || mIsDying || IsMindControlled() || !HasHead()
		|| !mBoard->SupportsWinterTemperature()
		|| mBoard->GetFrozenColumnCount() <= 0 || mBoard->HasIceWall();
}

void IceWallEngineerZombie::BeginConstruction(float frontierX)
{
	mConstructionPhase = ConstructionPhase::BUILDING;
	mConstructionRemaining = kConstructionDuration;
	mBuildWallCenterX = frontierX + IceWall::kBlockHalfWidth;
	mBuildParticleTimer = 0.0f;
	PlayTrack("anim_idle", 1.0f, 0.1f);
}

void IceWallEngineerZombie::CancelConstruction(bool consumeAbility)
{
	if (mConstructionPhase != ConstructionPhase::BUILDING) return;
	mConstructionUsed = mConstructionUsed || consumeAbility;
	mConstructionPhase = mConstructionUsed
		? ConstructionPhase::COMPLETED : ConstructionPhase::MOVING;
	mConstructionRemaining = 0.0f;
	mBuildParticleTimer = 0.0f;
	if (!mIsDying && IsActive()) PlayWalkAnimation(0.1f);
}

void IceWallEngineerZombie::CompleteConstruction()
{
	if (ShouldAbortConstruction()) {
		CancelConstruction(false);
		return;
	}
	if (!mBoard->AddIceWall(mRow, mBuildWallCenterX)) {
		CancelConstruction(false);
		return;
	}
	mConstructionUsed = true;
	mConstructionPhase = ConstructionPhase::COMPLETED;
	mConstructionRemaining = 0.0f;
	mBuildParticleTimer = 0.0f;
	PlayWalkAnimation(0.1f);
}

void IceWallEngineerZombie::OnMindControlled()
{
	CancelConstruction(true);
}

void IceWallEngineerZombie::ZombieItemUpdate() const
{
	ConeZombie::ZombieItemUpdate();
	ApplyEngineerEquipmentTextures();
}

void IceWallEngineerZombie::SaveExtraData(nlohmann::json& j) const
{
	ConeZombie::SaveExtraData(j);
	j["constructionPhase"] = static_cast<int>(mConstructionPhase);
	j["constructionRemaining"] = mConstructionRemaining;
	j["buildWallCenterX"] = mBuildWallCenterX;
	j["buildParticleTimer"] = mBuildParticleTimer;
	j["constructionUsed"] = mConstructionUsed;
}

void IceWallEngineerZombie::LoadExtraData(const nlohmann::json& j)
{
	ConeZombie::LoadExtraData(j);
	const int phase = std::clamp(j.value("constructionPhase", 0), 0,
		static_cast<int>(ConstructionPhase::COMPLETED));
	mConstructionPhase = static_cast<ConstructionPhase>(phase);
	mConstructionRemaining = std::clamp(
		j.value("constructionRemaining", 0.0f), 0.0f, kConstructionDuration);
	mBuildWallCenterX = j.value("buildWallCenterX", 0.0f);
	mBuildParticleTimer = std::clamp(
		j.value("buildParticleTimer", 0.0f), 0.0f, kBuildParticleInterval);
	mConstructionUsed = j.value("constructionUsed",
		mConstructionPhase == ConstructionPhase::COMPLETED);
	if (mConstructionPhase == ConstructionPhase::BUILDING
		&& (!HasHead() || IsMindControlled() || mConstructionRemaining <= 0.0f)) {
		mConstructionPhase = ConstructionPhase::MOVING;
		mConstructionRemaining = 0.0f;
		mConstructionUsed = !HasHead() || IsMindControlled();
	}
	ApplyEngineerEquipmentTextures();
	if (mConstructionPhase == ConstructionPhase::BUILDING && mAnimator) {
		PlayTrack("anim_idle", 1.0f, 0.0f);
	}
}

void IceWallEngineerZombie::SetConstructionStateForTesting(
	ConstructionPhase phase, float remaining, float wallCenterX, bool used)
{
	mConstructionPhase = phase;
	mConstructionRemaining = std::clamp(remaining, 0.0f, kConstructionDuration);
	mBuildWallCenterX = wallCenterX;
	mBuildParticleTimer = 0.0f;
	mConstructionUsed = used;
	if (mAnimator) {
		if (phase == ConstructionPhase::BUILDING) PlayTrack("anim_idle", 1.0f, 0.0f);
		else PlayWalkAnimation(0.0f);
	}
}

const std::string& IceWallEngineerZombie::GetConeTextureKey(
	ArmorBrokenState stage) const
{
	using namespace ResourceKeys::Textures;
	if (stage == ArmorBrokenState::A_LITTLE_BROKEN) {
		return IMAGE_ZOMBIE_ICEWALL_ENGINEER_HAT2;
	}
	if (stage == ArmorBrokenState::REALLY_BROKEN) {
		return IMAGE_ZOMBIE_ICEWALL_ENGINEER_HAT3;
	}
	return IMAGE_ZOMBIE_ICEWALL_ENGINEER_HAT1;
}

void IceWallEngineerZombie::ApplyEngineerEquipmentTextures() const
{
	if (!mAnimator) return;
	if (mAnimator->HasTrack("anim_cone")
		&& mHelmType != HelmType::HELMTYPE_NONE
		&& mHelmStage != ArmorBrokenState::NONE) {
		if (const Texture* hardhat = ResourceManager::GetInstance().GetTexture(
			GetConeTextureKey(mHelmStage), false)) {
			mAnimator->SetTrackImage("anim_cone", hardhat);
		}
	}
	if (mAnimator->HasTrack("Zombie_body")) {
		if (const Texture* body = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_ZOMBIE_ICEWALL_ENGINEER_BODY, false)) {
			mAnimator->SetTrackImage("Zombie_body", body);
		}
	}
	if (mAnimator->HasTrack("Zombie_tie")) {
		if (const Texture* toolStrap = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_ZOMBIE_ICEWALL_ENGINEER_TOOLSTRAP, false)) {
			mAnimator->SetTrackImage("Zombie_tie", toolStrap);
		}
	}
}
