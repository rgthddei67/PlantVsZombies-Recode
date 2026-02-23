#include "Coin.h"
#include "Board.h"
#include "../DeltaTime.h"

Coin::Coin(Board* board, AnimationType animType, const Vector& position,
	const Vector& colliderSize, const Vector& colliderOffset, float VanlishTime, 
	float scale,
	const std::string& tag, bool needScaleAnimation, bool autoDestroy)
	: AnimatedObject(ObjectType::OBJECT_COIN, board, position, animType, ColliderType::CIRCLE,
		colliderSize, colliderOffset, scale, tag, autoDestroy)
{
	this->mVanlishTime = VanlishTime;
	this->mTargetScale = scale;
	this->mStartScale = scale * 0.1f;
	this->mIsScaling = needScaleAnimation;
	this->mScaleAnimationFinished = !needScaleAnimation;
}

void Coin::Start()
{
	AnimatedObject::Start();
	if (mIsScaling) {
		SetScale(mStartScale);
	}
	else {
		SetScale(mTargetScale);
	}
	auto clickableComponent = AddComponent<ClickableComponent>();

	clickableComponent->ConsumeEvent = true;

	SetOnClickBack(clickableComponent);

	this->PlayTrack("Sun1");
}

void Coin::Update()
{
	UpdateScale();

	AnimatedObject::Update();

	if (isMovingToTarget)
	{
		Vector currentPos = GetPosition();
		Vector direction = targetPos - currentPos;
		float distance = direction.magnitude();

		if (distance < 65.0f)
		{
			OnReachTargetBack();
			return;
		}

		float currentSpeed = (distance > slowDownDistance) ? speedFast : speedSlow;

		if (distance > 0) {
			Vector normalizedDir = direction / distance;
			Vector newPos = currentPos + normalizedDir * currentSpeed * DeltaTime::GetDeltaTime();
			SetPosition(newPos);
		}
	}
	else
	{
		// 缩放动画完成前不开始消失计时
		if (!mScaleAnimationFinished) return;

		mVanlishTimer += DeltaTime::GetDeltaTime();
		if (mVanlishTimer >= mVanlishTime)
		{
			mVanlishTimer = 0.0f;
			GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
		}
	}
}

void Coin::SetOnClickBack(std::shared_ptr<ClickableComponent> clickComponent)
{
	if (clickComponent == nullptr) return;
	clickComponent->onClick = [this]() {
		StartMoveToTarget();
		};
}

void Coin::StartMoveToTarget(const Vector& target, float fastSpeed,
	float slowSpeed, float slowdownDist)
{
	PauseAnimation();
	if (auto clickable = GetComponent<ClickableComponent>()) {
		clickable->IsClickable = false;
	}
	targetPos = target;
	speedFast = fastSpeed;
	speedSlow = slowSpeed;
	slowDownDistance = slowdownDist;
	isMovingToTarget = true;
	StopScaleAnimation();
}

void Coin::StopMove()
{
	isMovingToTarget = false;
}

bool Coin::IsMoving() const
{
	return isMovingToTarget;
}

void Coin::SetTargetPosition(const Vector& target)
{
	targetPos = target;
}

void Coin::OnReachTargetBack()
{
	if (mIsDestroyed) return;
	isMovingToTarget = false;
	mIsDestroyed = true;
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

Vector Coin::GetPosition() const
{
	if (auto transform = mTransform.lock()) {
		return transform->GetPosition();
	}
	return Vector::zero();
}

void Coin::SetPosition(const Vector& newPos)
{
	if (auto transform = mTransform.lock()) {
		transform->SetPosition(newPos);
	}
}

void Coin::SetScale(float scale)
{
	if (auto transfrom = mTransform.lock()) {
		transfrom->SetScale(scale);
	}
}

void Coin::UpdateScale()
{
	if (!mIsScaling) return;

	mScaleTimer += DeltaTime::GetDeltaTime();
	float t = mScaleTimer / mScaleDuration;

	t = t * t * t; // 立方缓入，开始慢，结束快

	if (t >= 1.0f) {
		t = 1.0f;
		FinishScaleAnimation();
	}

	float currentScale = mStartScale + (mTargetScale - mStartScale) * t;
	SetScale(currentScale);
}

void Coin::StopScaleAnimation()
{
	mIsScaling = false;
	mScaleAnimationFinished = true;
}

void Coin::FinishScaleAnimation()
{
	StopScaleAnimation();
	SetScale(mTargetScale);
}