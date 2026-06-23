#include "AnimatedObject.h"
#include "Board.h"
#include "../DeltaTime.h"
#include "../GameRandom.h"
#include "../GameApp.h"
#include "../Logger.h"

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

	auto* transform = AddComponent<TransformComponent>();
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
		mAnimator->SetAlpha(1.0f);
		mAnimator->Play(mLoopType);
		mIsPlaying = true;
	}
	else {
		LOG_ERROR("AnimatedObject") << "cannot load animation for type "
			<< static_cast<int>(mAnimType);
	}

	PlayAnimation();
	SetAnimationSpeed(GameRandom::Range(1.0f, 1.2f));
}

Vector AnimatedObject::GetVisualPosition() const {
	if (mTransform) {
		return mTransform->GetPosition();
	}
	return Vector::zero();
}

void AnimatedObject::UpdateParallel(std::vector<DeferredEvent>& outBuf) {
	if (mAnimator) mAnimator->UpdateParallelDeferred(outBuf);
	mAdvancedInParallel = true;
}

void AnimatedObject::Update() {
	GameObject::Update();

	if (mAnimator) {
		if (mAdvancedInParallel) { mAdvancedInParallel = false; /* events 在 phase B drain 已处理；Animator 状态已就位 */ }
		else { mAnimator->Update(); }

		// 自动销毁逻辑（非循环动画且结束后自动销毁）
		if (mIsPlaying && mLoopType != PlayState::PLAY_REPEAT && IsAnimationFinished()) {
			if (mAutoDestroy) {
				GameObjectManager::GetInstance().DestroyGameObject(this);
			}
			else {
				mIsPlaying = false;
			}
		}
	}
	UpdateGlowingEffect();
}

void AnimatedObject::Draw(Graphics* g) {
	GameObject::Draw(g);
	if (!mAnimator) return;
	Vector pos = GetVisualPosition();
	float scale = mTransform ? mTransform->GetScale() : 1.0f;
	mAnimator->Draw(g, pos.x, pos.y, scale);
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
	if (mTransform) {
		mTransform->SetPosition(position);
	}
}

Vector AnimatedObject::GetAnimationPosition() const {
	if (mTransform) {
		return mTransform->GetPosition();
	}
	return Vector::zero();
}

void AnimatedObject::SetAnimationScale(float scale) {
	if (mTransform) {
		mTransform->SetScale(scale);
	}
}

float AnimatedObject::GetAnimationScale() const {
	if (mTransform) {
		return mTransform->GetScale();
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
	const std::string& returnTrack, float speed, float blendTime, float returnSpeed) {
	return mAnimator ? mAnimator->PlayTrackOnce(trackName, returnTrack, speed, blendTime, returnSpeed) : false;
}

float AnimatedObject::GetClipSpeed() const
{
	return mAnimator ? mAnimator->GetClipSpeed() : 0.0f;
}

void AnimatedObject::SetClipSpeed(float speed)
{
	if (mAnimator)
	{
		mAnimator->SetClipSpeed(speed);
	}
}

void AnimatedObject::SetFramesForLayer(const std::string& trackName) {
	if (mAnimator) {
		mAnimator->SetFrameRangeByTrackName(trackName);
	}
}

std::string AnimatedObject::GetCurrentTrackName() const {
	return mAnimator ? mAnimator->GetCurrentTrackName() : "";
}

float AnimatedObject::GetCurrentFrame() const {
	return mAnimator ? mAnimator->GetCurrentFrame() : 0.0f;
}

PlayState AnimatedObject::GetPlayingState() const {
	return mAnimator ? mAnimator->GetPlayingState() : PlayState::PLAY_REPEAT;
}

std::string AnimatedObject::GetTargetTrack() const {
	return mAnimator ? mAnimator->GetTargetTrack() : "";
}

float AnimatedObject::GetTargetTrackSpeed() const {
	return mAnimator ? mAnimator->GetTargetTrackSpeed() : 0.0f;
}

void AnimatedObject::SetCurrentFrame(float frameIndex) {
	if (mAnimator) {
		mAnimator->SetCurrentFrame(frameIndex);
	}
}

TransformComponent* AnimatedObject::GetTransformComponent() const {
	return mTransform;
}

ColliderComponent* AnimatedObject::GetColliderComponent() const {
	return mCollider;
}

std::shared_ptr<Animator> AnimatedObject::GetAnimatorInternal() const {
	return mAnimator;
}