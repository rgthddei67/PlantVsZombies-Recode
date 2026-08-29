#include "AuroraTorchwood.h"

#include "Game/Board/Board.h"
#include "../Bullet/Bullet.h"

#include <algorithm>

namespace {
	constexpr float kAuroraAttackOffsetX = 10.0f; // 与火炬树桩共用的转换区左偏移，单位：像素
	constexpr float kAuroraAttackWidth = 30.0f; // 棱晶冠横向转换判定宽度，单位：像素
	constexpr float kMinimumOverlap = 1.0f; // 至少重叠 1px 才视为真正穿过
}

void AuroraTorchwood::PlantUpdate()
{
	if (!mBoard) return;

	const float attackLeft = GetPosition().x + kAuroraAttackOffsetX;
	const float attackRight = attackLeft + kAuroraAttackWidth;
	for (const int bulletID : mBoard->mEntityRegistry.GetAllBulletIDs()) {
		Bullet* bullet = mBoard->mEntityRegistry.GetBullet(bulletID);
		if (!bullet || !bullet->IsActive() || bullet->mRow != mRow) continue;

		const ColliderComponent* collider = bullet->GetColliderComponent();
		if (!collider) continue;
		const SDL_FRect bounds = collider->GetBoundingBox();
		const float overlap = std::min(attackRight, bounds.x + bounds.w)
			- std::max(attackLeft, bounds.x);
		if (overlap < kMinimumOverlap) continue;

		bullet->ConvertToAuroraPea(mColumn);
	}
}
