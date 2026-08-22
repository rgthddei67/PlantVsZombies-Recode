#include "GoldMagnet.h"
#include "../../ParticleSystem/ParticleSystem.h"

#include "../Board.h"
#include "../Zombie/Zombie.h"

#include <algorithm>

namespace {
	constexpr float kRechargeSeconds = 12.0f; // 每次成功吸取后的总充能时间，单位：游戏秒
	constexpr float kEmpRadius = 100.0f; // EMP圆形判定半径，单位：px
	constexpr float kParalysisSeconds = 2.5f; // EMP麻痹持续时间，单位：游戏秒

	bool CircleOverlapsRect(const Vector& center, float radius,
		const SDL_FRect& bounds)
	{
		const float nearestX = std::clamp(center.x, bounds.x, bounds.x + bounds.w);
		const float nearestY = std::clamp(center.y, bounds.y, bounds.y + bounds.h);
		const float dx = center.x - nearestX;
		const float dy = center.y - nearestY;
		return dx * dx + dy * dy <= radius * radius;
	}
}

float GoldMagnet::GetRechargeSeconds() const
{
	return kRechargeSeconds;
}

float GoldMagnet::GetSimulationAbilityCooldownRemaining() const
{
	return GetPhase() == Phase::READY ? 0.0f : GetRechargeTimeRemaining();
}

void GoldMagnet::OnZombieMagneticItemExtracted(
	const MagneticItem&, const Vector& targetCenter, int)
{
	if (!mBoard) return;

	// EMP由实际被吸装备的僵尸位置结算；魅惑方与垂死实体不借此获得友军控制。
	for (int row = 0; row < mBoard->mRows; ++row) {
		mBoard->mEntityManager.ForEachZombieInRow(row, [&](Zombie* zombie) {
			if (!zombie || !zombie->IsActive() || zombie->IsDying()
				|| zombie->IsMindControlled()) return;
			const ColliderComponent* collider = zombie->GetColliderComponent();
			if (!collider || !CircleOverlapsRect(
				targetCenter, kEmpRadius, collider->GetBoundingBox())) return;
			zombie->ApplyParalysis(kParalysisSeconds);
		});
	}
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("GoldMagnetEMP", targetCenter);
	}
}
