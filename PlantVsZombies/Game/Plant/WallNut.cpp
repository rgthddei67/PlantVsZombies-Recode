#include "WallNut.h"

void WallNut::SetupPlant()
{
	Plant::SetupPlant();
	this->mPlantHealth = 4000;
	this->mPlantMaxHealth = 4000;
}

void WallNut::PlantUpdate()
{
	Plant::PlantUpdate();

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