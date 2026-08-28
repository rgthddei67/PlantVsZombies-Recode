#include "SeaShroom.h"

#include "../../ResourceKeys.h"
#include "../ShadowComponent.h"

namespace
{
	const Vector kSeaShroomBulletOffset(18.0f, 36.0f); // 孢子出生点相对公共水面视觉锚点的像素偏移
}

void SeaShroom::SetupPlant()
{
	// 原版海蘑菇漂在水面上，不绘制落在草地上的植物阴影。
	RemoveShadow();
	Shroom::SetupPlant();

	if (mIsPreview) return;

	mAnimator->AddFrameEvent(33, [this]() {
		if (!mBoard) return;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_PUFF, 0.28f);
		// 相对小喷菇发射点按原版海蘑菇口部差值校正，并跟随水面浮动。
		const Vector bulletPosition = GetVisualAnchorPosition() + kSeaShroomBulletOffset;
		mBoard->CreatePlantBullet(BulletType::BULLET_PUFF, mRow, bulletPosition, mPlantType);
	}, true);
}
