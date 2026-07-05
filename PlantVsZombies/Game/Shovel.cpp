#include "Shovel.h"
#include "Board.h"
#include "Cell.h"
#include "../UI/InputHandler.h"
#include "GameObjectManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../GameAPP.h"
#include <SDL2/SDL.h>

Shovel::Shovel(Board* board)
	: mBoard(board)
{
	// mPosition/mHomePosition(默认 Vector 即 0,0) 与 mState(IDLE) 均由头文件就地初始化
	mTexture = ResourceManager::GetInstance()
		.GetTexture(ResourceKeys::Textures::IMAGE_SHOVEL);
	this->mIsUI = true;
	this->SetRenderOrder(LAYER_UI + 50000);
}

void Shovel::Activate()
{
	mState = ShovelState::ACTIVE;
}

void Shovel::SetHomePosition(const Vector& pos)
{
	mHomePosition = pos;
	if (mState == ShovelState::IDLE)
		mPosition = mHomePosition;
}

void Shovel::ReturnHome()
{
	mState = ShovelState::IDLE;
	mPosition = mHomePosition;
	mPlant = nullptr;
}

void Shovel::Die()
{
	mState = ShovelState::IDLE;
	mPosition = mHomePosition;
	GameObjectManager::GetInstance().DestroyGameObject(this);
	mBoard->mShovel.reset();
	mPlant = nullptr;
}

void Shovel::Update()
{
	if (mState == ShovelState::IDLE)
		return;

	auto& input = GameAPP::GetInstance().GetInputHandler();
	mPosition = input.GetMouseWorldPosition();

	CheckPlant();

	if (input.IsMouseButtonPressed(SDL_BUTTON_RIGHT)) {
		ReturnHome();
		mBoard->mCursorObjectManager.ClearActive();
		return;
	}

	if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
		if (mPlant) {
			mPlant->Die();
			mPlant = nullptr;
			AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DELETEPLANT, 0.3f);
		}
		ReturnHome();
		mBoard->mCursorObjectManager.ClearActive();
	}
}

void Shovel::CheckPlant()
{
	for (auto& rowCells : mBoard->mCells) {
		for (auto& cell : rowCells) {
			if (cell && cell->ContainsPoint(mPosition) && !cell->IsEmpty()) {
				mPlant = mBoard->mEntityManager.GetPlant(cell->GetPlantID());
				mPlant->SetGlowingTimer(0.1f);
				return;
			}
		}
	}
	mPlant = nullptr;
}

void Shovel::Draw(Graphics* g)
{
	if (!mTexture || !g) return;
	g->DrawTexture(mTexture,
		mPosition.x - mTexture->width * 0.5f,
		mPosition.y - mTexture->height * 0.5f,
		static_cast<float>(mTexture->width),
		static_cast<float>(mTexture->height));
}