#include "WallNut.h"
#include "../ShadowComponent.h"

void WallNut::SetupPlant()
{
	Plant::SetupPlant();
	this->mPlantHealth = 4000;
	this->mPlantMaxHealth = 4000;

	if (auto shadowComponent = GetComponent<ShadowComponent>()) {
		shadowComponent->SetOffset(Vector(4, 26));
	}
}

void WallNut::PlantUpdate()
{
	Plant::PlantUpdate();

	// 被啃食时暂停动画，不再被啃食时恢复
	bool isBeingEaten = mEaterCount > 0;
	if (isBeingEaten && !mWasBeingEaten) {
		mAnimator->SetOriginalSpeed(mAnimator->GetSpeed());
		mAnimator->SetSpeed(0);
	}
	else if (!isBeingEaten && mWasBeingEaten) {
		mAnimator->RestoreSpeed();
	}
	mWasBeingEaten = isBeingEaten;

	mUpdateTextureTimer += DeltaTime::GetDeltaTime();

	if (mUpdateTextureTimer >= 0.5f) {
		mUpdateTextureTimer = 0.0f;
		UpdateTexture();
	}
}

void WallNut::UpdateTexture() 
{
	if (this->mPlantHealth <= this->mPlantMaxHealth / 3 * 2 &&
		this->mPlantHealth > this->mPlantMaxHealth / 3) {
		mAnimator->SetTrackImage("anim_face", ResourceManager::GetInstance().
			GetTexture("IMAGE_WALLNUT_CRACKED1"));
	}
	else if (this->mPlantHealth <= this->mPlantMaxHealth / 3) {
		mAnimator->SetTrackImage("anim_face", ResourceManager::GetInstance().
			GetTexture("IMAGE_WALLNUT_CRACKED2"));
	}
}