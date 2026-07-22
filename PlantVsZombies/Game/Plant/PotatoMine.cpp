#include "PotatoMine.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"
#include <algorithm>
#include <climits>

namespace {
	constexpr float kReadyDurationSeconds = 20.0f;	// 地雷从埋下到开始出土所需的基础游戏时间
	constexpr float kBoomCleanupSeconds = 2.0f;		// 压扁动画保留时间，结束后清理植物对象
	constexpr float kBlastRadius = 60.0f;			// 原版 KillAllZombiesInRadius 使用的爆炸半径
	constexpr float kBlastCenterOffsetX = -20.0f;	// 原版爆心相对当前格子中心的水平偏移
	constexpr float kBlastCenterOffsetY = -10.0f;	// C# 植物高 80，而本项目格子高 100，爆心需上移 10 像素
}

void PotatoMine::SetupPlant()
{
	if (auto shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(0, 23));
	}

	if (mIsPreview) return;

	GetColliderComponent()->onCollisionEnter =
		([this](ColliderComponent* other) {
		if (!mIsRise) return;

		auto* gameObject = other->GetGameObject();
		if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
		{
			if (auto zombie = dynamic_cast<Zombie*>(gameObject))
			{
				if (!this->mIsBoom && !zombie->IsMindControlled() && zombie->HasHead()) {
					Detonate();
				}
			}
		}
			});
}

void PotatoMine::PlantUpdate()
{
	float deltaTime = DeltaTime::GetDeltaTime();
	// 雨水只加速准备成长；爆炸后的销毁清理仍按真实游戏时间。
	mReadyTimer += GetWeatherActionDeltaTime();
	if (mReadyTimer >= kReadyDurationSeconds && !mIsRise) {
		mIsRise = true;
		Ready(false);
		// 碰撞进入只发生一次；若僵尸埋地时已经开啃，出土时必须主动补触发。
		if (mEaterCount > 0) {
			Detonate();
		}
	}

	if (mIsBoom) {
		mBoomTimer += deltaTime;
		if (mBoomTimer >= kBoomCleanupSeconds) {
			mBoomTimer = 0.0f;
			Die();
		}
	}
}

void PotatoMine::Detonate()
{
	if (mIsBoom || !mBoard) return;

	// 先锁住单次触发，再杀范围目标；僵尸 Die() 会回收本植物的 mEaterCount。
	mIsBoom = true;
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POTATO_MINE, 0.4f);
	PlayTrackOnce("anim_mashed", "");
	KillZombiesInBlastRadius();
}

void PotatoMine::KillZombiesInBlastRadius()
{
	const Vector blastCenter = GetPosition() + Vector(kBlastCenterOffsetX, kBlastCenterOffsetY);
	const float radiusSquared = kBlastRadius * kBlastRadius;

	// 镜像原版 KillAllZombiesInRadius：同排、圆形爆区，一次结算全部目标而非只杀碰撞触发者。
	mBoard->mEntityManager.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (zombie->IsMindControlled() || zombie->IsDying()) return;
		auto* collider = zombie->GetColliderComponent();
		if (!collider || !collider->mEnabled) return;

		const SDL_FRect bounds = collider->GetBoundingBox();
		const float nearestX = std::clamp(blastCenter.x, bounds.x, bounds.x + bounds.w);
		const float nearestY = std::clamp(blastCenter.y, bounds.y, bounds.y + bounds.h);
		const float dx = blastCenter.x - nearestX;
		const float dy = blastCenter.y - nearestY;
		if (dx * dx + dy * dy <= radiusSquared) {
			// 土豆雷仍对普通目标一击化灰；特殊目标可拒绝直杀并承受受限灰烬伤害。
			zombie->TakePlantAshDamage(INT32_MAX);
		}
		});
}

void PotatoMine::SaveExtraData(nlohmann::json& j) const
{
	j["readyTimer"] = mReadyTimer;
	j["isRise"] = mIsRise;
	j["boomTimer"] = mBoomTimer;
	j["isBoom"] = mIsBoom;
}

void PotatoMine::LoadExtraData(const nlohmann::json& j)
{
	mReadyTimer = j.value("readyTimer", 0.0f);
	mBoomTimer = j.value("boomTimer", 0.0f);
	mIsRise = j.value("isRise", false);
	mIsBoom = j.value("isBoom", false);

	if (mIsRise) {
		Ready(true);
	}
	if (mIsBoom) {
		PlayTrackOnce("anim_mashed", "");
	}
}

void PotatoMine::Ready(bool quick)
{
	if (!quick)
		PlayTrackOnce("anim_rise", "anim_armed");
	else
		PlayTrack("anim_armed");
}
