#include "TangleKelp.h"

#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"
#include "../../ParticleSystem/ParticleSystem.h"
#include <algorithm>
#include <cmath>

namespace {
	constexpr float kGrabDurationSeconds = 0.99f;       // 原版抓取总时长，单位：秒
	constexpr float kResistantHoldDurationSeconds = 5.0f; // 加固铁门抗拖沉后被原地束缚的时长，单位：秒
	constexpr float kDragThresholdSeconds = 0.51f;      // 跨过此剩余时间时停止啃食并开始下沉，单位：秒
	constexpr float kFinalSplashThresholdSeconds = 0.21f; // 跨过此剩余时间时播放末段水花与入水声，单位：秒
	constexpr float kAttackRectLeft = -40.0f;           // 近身检测矩形左边相对格中心的偏移，单位：像素
	constexpr float kAttackRectTop = -50.0f;            // 近身检测矩形上边相对格中心的偏移，单位：像素
	constexpr float kAttackRectWidth = 80.0f;            // 近身检测矩形宽度，单位：像素
	constexpr float kAttackRectHeight = 80.0f;           // 近身检测矩形高度，单位：像素
	constexpr float kSplashOffsetY = 25.0f;              // 水花相对逻辑位置向下的视觉偏移，单位：像素
}

void TangleKelp::SetupPlant()
{
	RemoveComponent<ShadowComponent>();
	if (mCollider) {
		mCollider->size = Vector(kAttackRectWidth, kAttackRectHeight);
		mCollider->offset = Vector(kAttackRectLeft, kAttackRectTop);
	}
}

void TangleKelp::PlantUpdate()
{
	if (mState == State::IDLE) {
		if (Zombie* target = FindTargetZombie()) {
			StartGrab(target);
		}
		return;
	}
	UpdateGrab();
}

Zombie* TangleKelp::FindTargetZombie() const
{
	if (!mBoard) return nullptr;

	const Vector position = GetPosition();
	const SDL_FRect attackRect{
		position.x + kAttackRectLeft,
		position.y + kAttackRectTop,
		kAttackRectWidth,
		kAttackRectHeight,
	};

	Zombie* best = nullptr;
	float bestX = 0.0f;
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (!zombie->CanBeTargetedByTangleKelp()) return;
		auto* collider = zombie->GetColliderComponent();
		if (!collider || !collider->mEnabled) return;
		const SDL_FRect bounds = collider->GetBoundingBox();
		const bool overlaps = attackRect.x < bounds.x + bounds.w
			&& attackRect.x + attackRect.w > bounds.x
			&& attackRect.y < bounds.y + bounds.h
			&& attackRect.y + attackRect.h > bounds.y;
		if (!overlaps) return;

		const float centerX = bounds.x + bounds.w * 0.5f;
		if (!best || centerX < bestX) {
			best = zombie;
			bestX = centerX;
		}
	});
	return best;
}

void TangleKelp::StartGrab(Zombie* target)
{
	if (!target || !target->StartTangleKelpGrab(mPlantID)) return;

	mState = State::GRABBING;
	mTargetResistsDrowning = target->ResistsTangleKelpDrowning();
	mGrabTimer = mTargetResistsDrowning
		? kResistantHoldDurationSeconds
		: kGrabDurationSeconds;
	mTargetZombieID = target->mZombieID;
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FLOOP, 0.25f);
	EmitSplashAt(target->GetPosition());
}

void TangleKelp::UpdateGrab()
{
	if (!mBoard) {
		Die();
		return;
	}

	Zombie* target = mBoard->mEntityManager.GetZombie(mTargetZombieID);
	if (!target || !target->IsTangleKelpTargetOf(mPlantID)) {
		mTargetZombieID = NULL_ZOMBIE_ID;
		Die();
		return;
	}

	// 兼容旧存档：旧版抓取中的加固铁门没有抗性字段，读档后从僵尸当前持门状态重建完整束缚。
	if (!mTargetResistsDrowning && target->ResistsTangleKelpDrowning()) {
		mTargetResistsDrowning = true;
		mGrabTimer = kResistantHoldDurationSeconds;
	}

	const float previous = mGrabTimer;
	const float deltaTime = mTargetResistsDrowning
		? DeltaTime::GetDeltaTime()
		: GetWeatherActionDeltaTime();
	mGrabTimer = std::max(0.0f, mGrabTimer - deltaTime);
	if (mTargetResistsDrowning) {
		if (mGrabTimer <= 0.0f) {
			Die();
		}
		return;
	}

	if (previous > kDragThresholdSeconds && mGrabTimer <= kDragThresholdSeconds) {
		target->DragUnderByTangleKelp(mPlantID);
		EmitSplashAt(target->GetPosition());
	}
	if (previous > kFinalSplashThresholdSeconds
		&& mGrabTimer <= kFinalSplashThresholdSeconds) {
		EmitSplashAt(GetPosition());
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_ZOMBIESPLASH, 0.45f);
	}
	if (mGrabTimer <= 0.0f) {
		Die();
	}
}

void TangleKelp::EmitSplashAt(const Vector& position) const
{
	if (g_particleSystem) {
		g_particleSystem->EmitEffect(
			"SquashPoolSplash", position + Vector(0.0f, kSplashOffsetY));
	}
}

void TangleKelp::Die()
{
	if (mDeathHandled) return;
	mDeathHandled = true;

	const int targetID = mTargetZombieID;
	mTargetZombieID = NULL_ZOMBIE_ID;
	if (mBoard && targetID != NULL_ZOMBIE_ID) {
		if (Zombie* target = mBoard->mEntityManager.GetZombie(targetID);
			target && target->IsTangleKelpTargetOf(mPlantID)) {
			if (mTargetResistsDrowning || target->ResistsTangleKelpDrowning()) {
				target->ReleaseTangleKelpGrab(mPlantID);
			}
			else {
				target->Die();
			}
		}
	}
	mTargetResistsDrowning = false;
	Plant::Die();
}

void TangleKelp::SaveExtraData(nlohmann::json& j) const
{
	j["state"] = static_cast<int>(mState);
	j["grabTimer"] = mGrabTimer;
	j["targetZombieID"] = mTargetZombieID;
	j["targetResistsDrowning"] = mTargetResistsDrowning;
}

void TangleKelp::LoadExtraData(const nlohmann::json& j)
{
	const int state = j.value("state", static_cast<int>(State::IDLE));
	mState = state == static_cast<int>(State::GRABBING)
		? State::GRABBING
		: State::IDLE;
	mTargetResistsDrowning = j.value("targetResistsDrowning", false);
	const float maxGrabDuration = mTargetResistsDrowning
		? kResistantHoldDurationSeconds
		: kGrabDurationSeconds;
	mGrabTimer = std::clamp(j.value("grabTimer", 0.0f), 0.0f, maxGrabDuration);
	mTargetZombieID = j.value("targetZombieID", NULL_ZOMBIE_ID);
	mDeathHandled = false;
	if (mState == State::IDLE) {
		mGrabTimer = 0.0f;
		mTargetZombieID = NULL_ZOMBIE_ID;
		mTargetResistsDrowning = false;
	}
}

const char* TangleKelp::GetTangleKelpStateName() const
{
	return mState == State::GRABBING ? "GRABBING" : "IDLE";
}

int TangleKelp::GetGrabTimeRemainingMs() const
{
	return static_cast<int>(std::lround(mGrabTimer * 1000.0f));
}
