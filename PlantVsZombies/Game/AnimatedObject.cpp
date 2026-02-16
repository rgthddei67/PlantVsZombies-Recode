#include "AnimatedObject.h"
#include "Board.h"
#include "../DeltaTime.h"
#include "../GameRandom.h"
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
    : GameObject(type), mBoard(board)
{
    SetTag(tag);

    auto transform = AddComponent<TransformComponent>();
    transform->position = position;
    transform->SetScale(scale);
    mTransform = transform;

    auto animation = AddComponent<ReanimationComponent>(animType, position, scale);
    if (animation) {
        animation->SetDrawOrder(80);
        animation->SetAutoDestroy(autoDestroy);
    }
    mAnimation = animation;

    if (colliderSize.x > 0 && colliderSize.y > 0) {
        mCollider = AddComponent<ColliderComponent>(colliderSize, colliderOffset, colliderType);
    }
}

void AnimatedObject::Start() {
    GameObject::Start();
    PlayAnimation();
    SetAnimationSpeed(GameRandom::Range(0.65f, 0.85f));
}

void AnimatedObject::Update() {
    GameObject::Update();
    UpdateGlowingEffect();
}

void AnimatedObject::UpdateGlowingEffect() {
    if (mGlowingTimer > 0.0f) {
        mGlowingTimer -= DeltaTime::GetDeltaTime();
        if (mGlowingTimer <= 0.0f) {
            EnableGlowEffect(false);
        }
    }
}

void AnimatedObject::PlayAnimation() {
    if (auto animation = mAnimation.lock()) {
        animation->Play();
    }
}

void AnimatedObject::PauseAnimation() {
    if (auto animation = mAnimation.lock()) {
        animation->Pause();
    }
}

void AnimatedObject::StopAnimation() {
    if (auto animation = mAnimation.lock()) {
        animation->Stop();
    }
}

void AnimatedObject::SetAnimationPosition(const Vector& position) {
    if (auto transform = mTransform.lock()) {
        transform->position = position;
        if (auto animation = mAnimation.lock()) {
            animation->SetPosition(position);
        }
    }
}

Vector AnimatedObject::GetAnimationPosition() const {
    if (auto transform = mTransform.lock()) {
        return transform->position;
    }
    return Vector::zero();
}

void AnimatedObject::SetAnimationScale(float scale) {
    if (auto transform = mTransform.lock()) {
        transform->SetScale(scale);
        if (auto animation = mAnimation.lock()) {
            animation->SetScale(scale);
        }
    }
}

float AnimatedObject::GetAnimationScale() const {
    if (auto transform = mTransform.lock()) {
        return transform->GetScale();
    }
    return 1.0f;
}

bool AnimatedObject::IsAnimationFinished() const {
    if (auto animation = mAnimation.lock()) {
        return animation->IsFinished();
    }
    return true;
}

bool AnimatedObject::IsAnimationPlaying() const {
    if (auto animation = mAnimation.lock()) {
        return animation->IsPlaying();
    }
    return false;
}

void AnimatedObject::SetAutoDestroy(bool autoDestroy) {
    if (auto animation = mAnimation.lock()) {
        animation->SetAutoDestroy(autoDestroy);
    }
}

void AnimatedObject::SetLoopType(PlayState loopType) {
    if (auto animation = mAnimation.lock()) {
        animation->SetLoopType(loopType);
    }
}

void AnimatedObject::SetAnimationSpeed(float speed) {
    if (auto animation = mAnimation.lock()) {
        animation->SetSpeed(speed);
    }
}

float AnimatedObject::GetAnimationSpeed() const {
    if (auto animation = mAnimation.lock()) {
        return animation->GetSpeed();
    }
    return 0.0f;
}

void AnimatedObject::SetAlpha(float alpha) {
    if (auto animation = mAnimation.lock()) {
        animation->SetAlpha(alpha);
    }
}

float AnimatedObject::GetAlpha() const {
    if (auto animation = mAnimation.lock()) {
        return animation->GetAlpha();
    }
    return 1.0f;
}

bool AnimatedObject::AttachAnimatorToTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
    if (auto animation = mAnimation.lock()) {
        return animation->AttachAnimatorToTrack(trackName, childAnimator);
    }
    return false;
}

void AnimatedObject::DetachAnimatorFromTrack(const std::string& trackName, std::shared_ptr<Animator> childAnimator) {
    if (auto animation = mAnimation.lock()) {
        animation->DetachAnimatorFromTrack(trackName, childAnimator);
    }
}

void AnimatedObject::DetachAllAnimators() {
    if (auto animation = mAnimation.lock()) {
        animation->DetachAllAnimators();
    }
}

void AnimatedObject::SetGlowingTimer(float duration) {
    mGlowingTimer = duration;
    EnableGlowEffect(duration > 0.0f);
}

void AnimatedObject::SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    if (auto animator = GetAnimatorInternal()) {
        animator->SetGlowColor(r, g, b, a);
    }
}

void AnimatedObject::EnableGlowEffect(bool enable) {
    if (auto animator = GetAnimatorInternal()) {
        animator->EnableGlowEffect(enable);
    }
}

void AnimatedObject::EnableOverlayEffect(bool enable) {
    if (auto animator = GetAnimatorInternal()) {
        animator->EnableOverlayEffect(enable);
    }
}

void AnimatedObject::OverrideColor(const SDL_Color& color) {
    if (auto animator = GetAnimatorInternal()) {
        animator->SetOverlayColor(color.r, color.g, color.b, color.a);
    }
}

bool AnimatedObject::PlayTrack(const std::string& trackName, float blendTime) {
    if (auto animation = mAnimation.lock()) {
        return animation->PlayTrack(trackName, blendTime);
    }
    return false;
}

bool AnimatedObject::PlayTrackOnce(const std::string& trackName, const std::string& returnTrack, float speed, float blendTime) {
    if (auto animation = mAnimation.lock()) {
        return animation->PlayTrackOnce(trackName, returnTrack, speed, blendTime);
    }
    return false;
}

void AnimatedObject::SetFramesForLayer(const std::string& trackName) {
    if (auto animation = mAnimation.lock()) {
        animation->SetFramesForLayer(trackName);
    }
}

std::shared_ptr<ReanimationComponent> AnimatedObject::GetAnimationComponent() const {
    return mAnimation.lock();
}

std::shared_ptr<TransformComponent> AnimatedObject::GetTransformComponent() const {
    return mTransform.lock();
}

std::shared_ptr<ColliderComponent> AnimatedObject::GetColliderComponent() const {
    return mCollider.lock();
}

std::shared_ptr<Animator> AnimatedObject::GetAnimator() const {
    return GetAnimatorInternal();
}

std::shared_ptr<Animator> AnimatedObject::GetAnimatorInternal() const {
    if (auto animation = mAnimation.lock()) {
        return animation->GetAnimator();
    }
    return nullptr;
}