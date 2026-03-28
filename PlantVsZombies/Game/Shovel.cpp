#include "Shovel.h"
#include "Board.h"
#include "Cell.h"
#include "../UI/InputHandler.h"
#include "GameObjectManager.h"
#include "../ResourceManager.h"
#include "../ResourceKeys.h"
#include "../GameApp.h"
#include <SDL2/SDL.h>

Shovel::Shovel(Board* board)
    : mBoard(board)
    , mPosition(0.0f, 0.0f)
    , mHomePosition(0.0f, 0.0f)
    , mState(ShovelState::IDLE)
{
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
    mState    = ShovelState::IDLE;
    mPosition = mHomePosition;
}

void Shovel::Die()
{
    mState = ShovelState::IDLE;
    mPosition = mHomePosition;
    GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
    mBoard->mShovel.reset();
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
        return;
    }

    if (input.IsMouseButtonPressed(SDL_BUTTON_LEFT)) {
        if (auto plant = mPlant.lock()) {
            plant->Die();
            AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_DELETEPLANT, 0.3f);
        }
        ReturnHome();
    }
}

void Shovel::CheckPlant()
{
    for (auto& rowCells : mBoard->mCells) {
        for (auto& cell : rowCells) {
            if (cell && cell->ContainsPoint(mPosition) && !cell->IsEmpty()) {
                mPlant = mBoard->mEntityManager.GetPlant(cell->GetPlantID());
                mPlant.lock()->SetGlowingTimer(0.1f);
                return;
            }
        }
    }
    mPlant.reset();
}

void Shovel::Draw(Graphics* g)
{
    if (!mTexture || !g) return;
    g->DrawTexture(mTexture,
        mPosition.x - mTexture->width  * 0.5f,
        mPosition.y - mTexture->height * 0.5f,
        static_cast<float>(mTexture->width),
        static_cast<float>(mTexture->height));
}
