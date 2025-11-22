#include "Coin.h"
#include "Board.h"
#include "../DeltaTime.h"

Coin::Coin(Board* board, AnimationType animType, const Vector& position,
    const Vector& colliderSize, float VanlishTime, float scale,
    const std::string& tag, bool autoDestroy)
    : AnimatedObject(board, position, animType, ColliderType::CIRCLE,
        colliderSize, scale, tag, autoDestroy)
{
	this->mVanlishTime = VanlishTime;
}

void Coin::Start()
{
    AnimatedObject::Start();
    auto clickableComponent = AddComponent<ClickableComponent>();
    SetOnClickBack(clickableComponent);
}

void Coin::Update()
{
    AnimatedObject::Update();

    // 在移动的时候，不销毁
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
            Vector newPos = currentPos + normalizedDir * currentSpeed * 0.018f;
            SetPosition(newPos);
        }
    }
    else
    {
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
    isMovingToTarget = false;
    GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}

Vector Coin::GetPosition() const
{
    if (mTransform) {
        return mTransform->position;
    }
    return Vector::zero();
}

void Coin::SetPosition(const Vector& newPos)
{
    if (mTransform) {
        mTransform->SetPosition(newPos);
    }
}