#include "FrostMine.h"

#include "../../GameApp.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <algorithm>

namespace {
	constexpr int kBodyDamage = 600;                    // 爆裂对目标标准防具链的植物来源伤害
	constexpr int kIceEquipmentCorrosion = 1000;        // 只进入目标自有冰制层，溢出不得灌入本体
	constexpr float kMineSoundVolume = 0.38f;            // 土豆雷爆破底音量
	constexpr float kFrostSoundVolume = 0.22f;           // 冰裂叠音量，强化寒霜身份但不盖住爆破
	constexpr const char* kBodyTrack = "FrostMine_body"; // 三态完整立绘共用的 reanim 轨道
}

void FrostMine::SetupPlant()
{
	if (auto* shadow = GetShadow()) {
		shadow->SetOffset(Vector(0.0f, 24.0f));
	}
	RefreshPresentation();

	if (mIsPreview || !GetColliderComponent()) return;
	GetColliderComponent()->SetCollisionEnterCallback(
		[this](ColliderComponent* other) {
			if (mPhase != Phase::ARMED || !other) return;
			GameObject* object = other->GetGameObject();
			if (!object || object->GetObjectType() != ObjectType::OBJECT_ZOMBIE) return;
			if (auto* zombie = dynamic_cast<Zombie*>(object)) DetonateOn(*zombie);
		});
}

void FrostMine::PlantUpdate()
{
	if (!mBoard || mPhase == Phase::SPENT) return;

	if (mPhase == Phase::DORMANT
		&& mBoard->IsCellInColdWaveForecast(mRow, mColumn)) {
		mPhase = Phase::CALIBRATED;
		RefreshPresentation();
	}
	if (mPhase == Phase::CALIBRATED && mBoard->IsCellFrozen(mRow, mColumn)) {
		Arm();
	}
	// 碰撞对可能在校准前已经存在；埋伏后主动扫描能保证冻结边沿当帧补触发。
	if (mPhase == Phase::ARMED) {
		if (Zombie* zombie = FindOverlappingTarget()) DetonateOn(*zombie);
	}
}

void FrostMine::OnColdWaveForecastDisrupted()
{
	// 已经进入真实冻土的埋伏属于已提交动作，预报干扰只能清除尚未兑现的校准。
	if (mPhase != Phase::CALIBRATED) return;
	mPhase = Phase::DORMANT;
	RefreshPresentation();
}

bool FrostMine::CanBeEaten() const
{
	return mPhase != Phase::ARMED && Plant::CanBeEaten();
}

void FrostMine::SaveExtraData(nlohmann::json& j) const
{
	j["phase"] = static_cast<int>(mPhase);
}

void FrostMine::LoadExtraData(const nlohmann::json& j)
{
	const int savedPhase = std::clamp(j.value("phase", 0),
		static_cast<int>(Phase::DORMANT), static_cast<int>(Phase::SPENT));
	mPhase = static_cast<Phase>(savedPhase);
	// SPENT 与 Die() 在同一调用栈内提交，正常存档不会看到它；损坏档恢复为仍可触发的埋伏。
	if (mPhase == Phase::SPENT) mPhase = Phase::ARMED;
	RefreshPresentation();
}

const char* FrostMine::GetPhaseName() const
{
	switch (mPhase) {
	case Phase::DORMANT: return "DORMANT";
	case Phase::CALIBRATED: return "CALIBRATED";
	case Phase::ARMED: return "ARMED";
	case Phase::SPENT: return "SPENT";
	}
	return "DORMANT";
}

void FrostMine::RefreshPresentation()
{
	if (!mAnimator) return;
	const std::string* textureKey = nullptr;
	switch (mPhase) {
	case Phase::DORMANT:
		textureKey = &ResourceKeys::Textures::IMAGE_REANIM_FROSTMINE_DORMANT;
		break;
	case Phase::CALIBRATED:
		textureKey = &ResourceKeys::Textures::IMAGE_REANIM_FROSTMINE_CALIBRATED;
		break;
	case Phase::ARMED:
	case Phase::SPENT:
		textureKey = &ResourceKeys::Textures::IMAGE_REANIM_FROSTMINE_ARMED;
		break;
	}
	const Texture* texture = ResourceManager::GetInstance().GetTexture(*textureKey);
	mAnimator->SetTrackImage(kBodyTrack, texture);
}

void FrostMine::Arm()
{
	if (mPhase != Phase::CALIBRATED) return;
	mPhase = Phase::ARMED;
	RefreshPresentation();
}

Zombie* FrostMine::FindOverlappingTarget() const
{
	if (!mBoard || !GetColliderComponent()) return nullptr;
	const ColliderComponent* mineCollider = GetColliderComponent();
	if (!mineCollider->mEnabled) return nullptr;
	const SDL_FRect mineBounds = mineCollider->GetBoundingBox();

	Zombie* selected = nullptr;
	mBoard->mEntityRegistry.ForEachZombieInRow(mRow, [&](Zombie* zombie) {
		if (!zombie || !zombie->IsActive() || zombie->IsDying()
			|| zombie->IsMindControlled() || !zombie->CanBeAffectedByGroundHazards()) return;
		const ColliderComponent* zombieCollider = zombie->GetColliderComponent();
		if (!zombieCollider || !zombieCollider->mEnabled) return;
		const SDL_FRect zombieBounds = zombieCollider->GetBoundingBox();
		if (SDL_HasIntersectionF(&mineBounds, &zombieBounds) != SDL_TRUE) return;
		if (!selected || zombie->mZombieID < selected->mZombieID) selected = zombie;
	});
	return selected;
}

void FrostMine::DetonateOn(Zombie& zombie)
{
	if (mPhase != Phase::ARMED || !mBoard || !zombie.IsActive()
		|| zombie.IsDying() || zombie.IsMindControlled()
		|| !zombie.CanBeAffectedByGroundHazards()) return;

	// 先锁定爆点与单次状态；后续腐蚀可删除装备 Animator，伤害也可能让目标进入死亡轨道。
	const ColliderComponent* zombieCollider = zombie.GetColliderComponent();
	Vector impactPosition = GetPosition() + Vector(0.0f, -16.0f);
	if (zombieCollider) {
		const SDL_FRect bounds = zombieCollider->GetBoundingBox();
		impactPosition = Vector(bounds.x + bounds.w * 0.5f,
			bounds.y + bounds.h * 0.65f);
	}
	mPhase = Phase::SPENT;

	// 中断必须早于破坏装备：目标仍能准确区分“尚未提交动作”与装备耗尽后的终止状态。
	zombie.InterruptUncommittedSpecialAction();
	zombie.ApplyWinterCorrosion(kIceEquipmentCorrosion);
	zombie.TakeDamage(kBodyDamage, DamageSource::PLANT,
		false, false, false, PlantDamageOrigin::FromPlant(mPlantType));

	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POTATO_MINE, kMineSoundVolume);
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FROZEN, kFrostSoundVolume);
	if (g_particleSystem) g_particleSystem->EmitEffect("FrostMineBurst", impactPosition);
	Die();
}
