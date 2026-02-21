#include "AnimatedObject.h"
#include "Board.h"
#include "../DeltaTime.h"
#include "../GameRandom.h"
#include "../GameAPP.h"
#include <iostream>

AnimatedObject::AnimatedObject(ObjectType type,
    Board* board,
    const Vector& position,
    AnimationType animType,
    const ColliderType& colliderType,
    const Vector& colliderSize,
    const Vector& colliderOffset,
    float scale,
    const std::string& tag,
    bool autoDestroy)
    : GameObject(type)
    , mBoard(board)
    , mAnimType(animType)
    , mIsPlaying(false)
    , mLoopType(PlayState::PLAY_REPEAT)
    , mAutoDestroy(autoDestroy)
{
    SetTag(tag);

    auto transform = AddComponent<TransformComponent>();
    transform->SetPosition(position);
    transform->SetScale(scale);
    mTransform = transform;

    if (colliderSize.x > 0 && colliderSize.y > 0) {
        mCollider = AddComponent<ColliderComponent>(colliderSize, colliderOffset, colliderType);
    }

    ResourceManager& resMgr = ResourceManager::GetInstance();
    auto reanimResource = resMgr.GetReanimation(resMgr.AnimationTypeToString(mAnimType));
    if (reanimResource) {
        mAnimator = std::make_shared<Animator>(reanimResource);
        mAnimator->SetSpeed(1.0f);
        mAnimator->SetAlpha(1.0f);
        mAnimator->Play(mLoopType);
        mIsPlaying = true;
    }
    else {
        std::cerr << "AnimatedObject::Start failed: cannot load animation for type "
            << static_cast<int>(mAnimType) << std::endl;
    }

    PlayAnimation();
    SetAnimationSpeed(GameRandom::Range(0.65f, 0.85f));
}

Vector AnimatedObject::GetVisualPosition() const {
    if (auto transform = mTransform.lock()) {
        return transform->GetPosition();;
    }
    return Vector::zero();
}

void AnimatedObject::Update() {
    GameObject::Update();

    if (mAnimator) {
        mAnimator->Update();

        // 自动销毁逻辑（非循环动画且结束后自动销毁）
        if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
            if (mAutoDestroy) {
                GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
            }
            else {
                mIsPlaying = false;
            }
        }
    }

    UpdateGlowingEffect();
}

void AnimatedObject::Draw(SDL_Renderer* renderer) {
    GameObject::Draw(renderer);
    if (!mAnimator) return;

    Vector screenPos = GameAPP::GetInstance().GetCamera().WorldToScreen
        (GetVisualPosition());
    float scale = 1.0f;

    if (auto transform = mTransform.lock()) {
        scale = transform->GetScale();
    }
    mAnimator->Draw(renderer, screenPos.x, screenPos.y, scale);
}

void AnimatedObject::PlayAnimation() {
    if (mAnimator) {
        mAnimator->Play(mLoopType);
        mIsPlaying = true;
    }
}

void AnimatedObject::PauseAnimation() {
    if (mAnimator) {
        mAnimator->Pause();
        mIsPlaying = false;
    }
}

void AnimatedObject::StopAnimation() {
    if (mAnimator) {
        mAnimator->Stop();
        mIsPlaying = false;
    }
}

void AnimatedObject::SetAnimationPosition(const Vector& position) {
    if (auto transform = mTransform.lock()) {
        transform->SetPosition(position);
    }
}

Vector AnimatedObject::GetAnimationPosition() const {
    if (auto transform = mTransform.lock()) {
        return transform->GetPosition();
    }
    return Vector::zero();
}

void AnimatedObject::SetAnimationScale(float scale) {
    if (auto transform = mTransform.lock()) {
        transform->SetScale(scale);
    }
}

float AnimatedObject::GetAnimationScale() const {
    if (auto transform = mTransform.lock()) {
        return transform->GetScale();
    }
    return 1.0f;
}

bool AnimatedObject::IsAnimationFinished() const {
    if (!mAnimator) return true;
    if (mLoopType == PlayState::PLAY_REPEAT) return false;
    return !mAnimator->IsPlaying();
}

bool AnimatedObject::IsAnimationPlaying() const {
    return mIsPlaying && mAnimator && mAnimator->IsPlaying();
}

void AnimatedObject::SetAutoDestroy(bool autoDestroy) {
    mAutoDestroy = autoDestroy;
}

void AnimatedObject::SetLoopType(PlayState loopType) {
    mLoopType = loopType;
    if (mIsPlaying && mAnimator) {
        mAnimator->Play(mLoopType);
    }
}

void AnimatedObject::SetAnimationSpeed(float speed) {
    if (mAnimator) {
        mAnimator->SetSpeed(speed);
    }
}

float AnimatedObject::GetAnimationSpeed() const {
    return mAnimator ? mAnimator->GetSpeed() : 0.0f;
}

void AnimatedObject::SetAlpha(float alpha) {
    if (mAnimator) {
        mAnimator->SetAlpha(alpha);
    }
}

float AnimatedObject::GetAlpha() const {
    return mAnimator ? mAnimator->GetAlpha() : 1.0f;
}

bool AnimatedObject::AttachAnimatorToTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
    return mAnimator ? mAnimator->AttachAnimator(trackName, childAnimator) : false;
}

void AnimatedObject::DetachAnimatorFromTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
    if (mAnimator) {
        mAnimator->DetachAnimator(trackName, childAnimator);
    }
}

void AnimatedObject::DetachAllAnimators() {
    if (mAnimator) {
        mAnimator->DetachAllAnimators();
    }
}

void AnimatedObject::UpdateGlowingEffect() {
    if (mGlowingTimer > 0.0f) {
        mGlowingTimer -= DeltaTime::GetDeltaTime();
        if (mGlowingTimer <= 0.0f) {
            EnableGlowEffect(false);
        }
    }
}

void AnimatedObject::SetGlowingTimer(float duration) {
    mGlowingTimer = duration;
    EnableGlowEffect(duration > 0.0f);
}

void AnimatedObject::SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (mAnimator) {
        mAnimator->SetGlowColor(r, g, b, a);
    }
}

void AnimatedObject::EnableGlowEffect(bool enable) {
    if (mAnimator) {
        mAnimator->EnableGlowEffect(enable);
    }
}

void AnimatedObject::EnableOverlayEffect(bool enable) {
    if (mAnimator) {
        mAnimator->EnableOverlayEffect(enable);
    }
}

void AnimatedObject::OverrideColor(const SDL_Color& color) {
    if (mAnimator) {
        mAnimator->SetOverlayColor(color.r, color.g, color.b, color.a);
    }
}

bool AnimatedObject::PlayTrack(const std::string& trackName, float speed, float blendTime) {
    return mAnimator ? mAnimator->PlayTrack(trackName, speed, blendTime) : false;
}

bool AnimatedObject::PlayTrackOnce(const std::string& trackName, 
    const std::string& returnTrack, float speed, float blendTime) {
    return mAnimator ? mAnimator->PlayTrackOnce(trackName, returnTrack, speed, blendTime) : false;
}

float AnimatedObject::GetOriginalSpeed()
{
    if (mAnimator)
    {
        return mAnimator->GetOriginalSpeed();
    }
    return 0.0f;
}

void AnimatedObject::SetOriginalSpeed(float speed)
{
    if (mAnimator)
    {
        mAnimator->SetOriginalSpeed(speed);
    }
}

void AnimatedObject::SetFramesForLayer(const std::string& trackName) {
    if (mAnimator) {
        mAnimator->SetFrameRangeByTrackName(trackName);
    }
}

std::shared_ptr<TransformComponent> AnimatedObject::GetTransformComponent() const {
    return mTransform.lock();
}

std::shared_ptr<ColliderComponent> AnimatedObject::GetColliderComponent() const {
    return mCollider.lock();
}

std::shared_ptr<Animator> AnimatedObject::GetAnimatorInternal() const {
    return mAnimator;
}