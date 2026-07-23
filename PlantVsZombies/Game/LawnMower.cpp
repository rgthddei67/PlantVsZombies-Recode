#include "LawnMower.h"
#include "Board.h"
#include "Zombie/Zombie.h"
#include "GameObjectManager.h"
#include "ShadowComponent.h"
#include "../DeltaTime.h"
#include "../GameAPP.h"
#include <algorithm>

namespace {
	constexpr float kPoolCleanerClipSpeed = 35.0f / 12.0f; // 原版 35 FPS；本引擎参数是相对资源 12 FPS 的倍率
	constexpr float kPoolCleanerMoveSpeed = 230.0f * 2.5f / 3.33f; // 按原版水车/草车 2.5:3.33 的速度比换算
	constexpr float kPoolTransitionDepth = 28.0f;           // 入水阶段最大视觉下沉距离，单位：像素
	constexpr float kPoolTransitionSpeedRatio = 0.8f;       // 原版每横移 2.5 像素下沉/上浮 2 像素
	constexpr float kPoolMowBlendTime = 0.1f;               // 吞噬轨道切换混合时间，单位：秒
}

Mower::Mower(Board* board, MowerType type, AnimationType animType, float x, float y, int row, float scale)
	: AnimatedObject(ObjectType::OBJECT_LAWNMOWER, board,
		Vector(x, y),
		animType,
		ColliderType::BOX,
		Vector(60, 50),
		Vector(0, 0),
		scale,
		"LawnMower",
		false)
{
	this->mRow = row;
	this->mMowerType = type;

	auto shadowcomponent = AddComponent<ShadowComponent>
		(ResourceManager::GetInstance().GetTexture
		(ResourceKeys::Textures::IMAGE_PLANTSHADOW));
	shadowcomponent->SetDrawOrder(-80);
	shadowcomponent->SetOffset(Vector(40, 50));
	shadowcomponent->SetScale(Vector(1.0f, 1.0f));

	// 水路清洁车待机时停在陆地轨道首帧，启动后才以原版速率循环。
	if (mMowerType == MowerType::WATER) {
		mSpeed = kPoolCleanerMoveSpeed;
		PlayTrack("anim_land", kPoolCleanerClipSpeed);
		PauseAnimation();
	}

	auto collider = GetColliderComponent();
	if (!collider) return;

	collider->isTrigger = true;
	collider->layerMask = CollisionLayer::MOWER;
	collider->collisionMask = CollisionLayer::ZOMBIE;
	collider->onTriggerStay = [this](ColliderComponent* other) {
		auto* go = other->GetGameObject();
		if (!go || go->GetObjectType() != ObjectType::OBJECT_ZOMBIE) return;
		auto* zombie = dynamic_cast<Zombie*>(go);
		if (!zombie) return;

		// 首次碰撞触发移动
		if (mState == MowerState::IDLE) {
			Trigger();
		}

		// 精英所在行保留正常启动和秒杀；其余行只失去小推车，不播放启动效果。
		if (zombie->ConsumesOtherMowersOnContact() && mBoard) {
			mBoard->RemoveOtherMowersWithoutTrigger(mMowerID);
		}

		if (mMowerType == MowerType::WATER) {
			PlayPoolMowAnimation();
		}
		zombie->TakeDamage(INT32_MAX, DamageSource::OTHER);

		};
}

void Mower::Trigger()
{
	if (mState == MowerState::MOVING) return;

	if (mMowerType == MowerType::WATER) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POOL_CLEANER, 0.4f);
		PlayTrack("anim_land", kPoolCleanerClipSpeed);
	}
	else {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_LAWNMOWER, 0.4f);
		PlayTrack("anim_normal");
	}

	mState = MowerState::MOVING;
	if (mBoard) {
		mBoard->SetRowLoseMower(mRow);
	}
}

void Mower::Update()
{
	// 通用动画读档会恢复轨道状态但不保存 Pause；待机水路车每帧重申首帧冻结契约。
	if (mMowerType == MowerType::WATER && mState == MowerState::IDLE) {
		PauseAnimation();
	}
	AnimatedObject::Update();

	if (mState != MowerState::MOVING) return;

	float deltaTime = DeltaTime::GetDeltaTime();
	Vector pos = GetPosition();
	pos.x += mSpeed * deltaTime;
	SetPosition(pos);
	if (mMowerType == MowerType::WATER) {
		UpdatePoolState(deltaTime);
	}

	if (pos.x > static_cast<float>(SCENE_WIDTH) + 40.0f) {
		Die();
	}
}

void Mower::Die()
{
	if (mCollider) {
		mCollider->mEnabled = false;
	}
	GameObjectManager::GetInstance().DestroyGameObject(this);
}

Vector Mower::GetPosition() const
{
	if (mTransform) {
		return mTransform->GetPosition();
	}
	return Vector::zero();
}

void Mower::SetPosition(const Vector& position)
{
	if (mTransform) {
		mTransform->SetPosition(position);
	}
}

Vector Mower::GetVisualPosition() const
{
	return AnimatedObject::GetVisualPosition() + Vector(0.0f, mPoolVisualOffsetY);
}

void Mower::RestorePoolVisualState(MowerHeight height, float visualOffsetY)
{
	mMowerHeight = height;
	mPoolVisualOffsetY = std::clamp(visualOffsetY, 0.0f, kPoolTransitionDepth);
	UpdatePoolShadowVisibility();
}

void Mower::UpdatePoolState(float deltaTime)
{
	if (!mBoard) return;

	const Vector pos = GetPosition();
	const bool overPool = mBoard->IsPoolWorldPosition(mRow, pos.x + 40.0f);
	const float transitionStep = mSpeed * kPoolTransitionSpeedRatio * deltaTime;

	switch (mMowerHeight) {
	case MowerHeight::LAND:
		if (overPool) {
			mMowerHeight = MowerHeight::ENTERING;
			mPoolVisualOffsetY = 0.0f;
			UpdatePoolShadowVisibility();
		}
		break;

	case MowerHeight::ENTERING:
		// 逻辑位置和碰撞体沿原行移动；只有绘制位置逐步沉入水面。
		mPoolVisualOffsetY = std::min(
			kPoolTransitionDepth, mPoolVisualOffsetY + transitionStep);
		if (mPoolVisualOffsetY >= kPoolTransitionDepth) {
			mMowerHeight = MowerHeight::IN_POOL;
			mPoolVisualOffsetY = kPoolTransitionDepth;
			PlayTrack("anim_water", kPoolCleanerClipSpeed);
			UpdatePoolShadowVisibility();
		}
		break;

	case MowerHeight::IN_POOL:
		if (!overPool) {
			mMowerHeight = MowerHeight::EXITING;
			mPoolVisualOffsetY = kPoolTransitionDepth;
			PlayTrack("anim_land", kPoolCleanerClipSpeed);
			UpdatePoolShadowVisibility();
		}
		break;

	case MowerHeight::EXITING:
		mPoolVisualOffsetY = std::max(0.0f, mPoolVisualOffsetY - transitionStep);
		if (mPoolVisualOffsetY <= 0.0f) {
			mMowerHeight = MowerHeight::LAND;
			mPoolVisualOffsetY = 0.0f;
			UpdatePoolShadowVisibility();
		}
		break;
	}
}

void Mower::PlayPoolMowAnimation()
{
	const bool inPool = mMowerHeight == MowerHeight::IN_POOL;
	const char* mowTrack = inPool ? "anim_suck" : "anim_landsuck";
	const char* returnTrack = inPool ? "anim_water" : "anim_land";
	PlayTrackOnce(mowTrack, returnTrack,
		kPoolCleanerClipSpeed, kPoolMowBlendTime, kPoolCleanerClipSpeed);
}

void Mower::UpdatePoolShadowVisibility()
{
	if (auto* shadow = GetComponent<ShadowComponent>()) {
		shadow->SetVisible(mMowerHeight == MowerHeight::LAND);
	}
}
