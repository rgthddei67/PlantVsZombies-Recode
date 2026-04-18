#include "PotatoMine.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

void PotatoMine::SetupPlant()
{
	if (auto shadow = GetComponent<ShadowComponent>()) {
		shadow->SetOffset(Vector(0, 23));
	}

	if (mIsPreview) return;

	GetColliderComponent()->onCollisionEnter = 
		([this](std::shared_ptr<ColliderComponent> other) {
		auto gameObject = other->GetGameObject().get();
		if (gameObject->GetObjectType() == ObjectType::OBJECT_ZOMBIE)
		{
			if (auto zombie = dynamic_cast<Zombie*>(gameObject))
			{
				if (!zombie->IsMindControlled() && zombie->HasHead()) {
					AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_POTATO_MINE, 0.4f);
					PlayTrackOnce("anim_mashed", "");
					this->mIsBoom = true;
					zombie->Die();
				}
			}
		}
		});
}

void PotatoMine::PlantUpdate()
{
	float deltaTime = DeltaTime::GetDeltaTime();
	mReadyTimer += deltaTime;
	if (mReadyTimer >= 20.0f && !mIsRise) {
		mIsRise = true;
		Ready(false);
	}

	if (mIsBoom) {
		mBoomTimer += deltaTime;
		if (mBoomTimer >= 2.0f) {
			mBoomTimer = 0.0f;
			Die();
		}
	}
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