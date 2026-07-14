#include "IceShroom.h"
#include "../GameScene.h"
#include "../AudioSystem.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

void IceShroom::SetupPlant()
{
	auto shadow = GetComponent<ShadowComponent>();
	shadow->SetScale(Vector(0.6f, 0.6f));
	shadow->SetOffset(Vector(-4, 30));

	Shroom::SetupPlant();

	// 预览/图鉴绝不结算（主人特别叮嘱：图鉴里也会走到这里）
	if (mIsPreview) return;

	// 第 16 帧全场冻结（主人指定，帧号已按代码口径 = 预览帧号-1，直接使用）。
	// 存档在触发前：SetupPlant 读档时重新注册，RestoreAnimState 恢复的帧在其前，穿过时照常触发。
	mAnimator->AddFrameEvent(16, [this]() {
		if (!mBoard) return;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_FROZEN, 0.5f);
		if (mBoard->mGameScene) mBoard->mGameScene->ShowScreenFlash(0.5f);
		FreezeAllZombies();
		Die();
		});
}

void IceShroom::FreezeAllZombies()
{
	// "全场" = 0..mRows-1 逐行行桶（无整场 API；行索引已排除失活/濒死）。
	// 定身+减速尾巴+20 点伤害全在 StartFrozen 内统一结算（镜像原版 HitIceTrap：
	// 豁免者——魅惑/出土伴舞/跳跃中撑杆——连伤害也不吃）。
	for (int row = 0; row < mBoard->mRows; ++row) {
		mBoard->mEntityManager.ForEachZombieInRow(row, [](Zombie* zombie) {
			zombie->StartFrozen();
			});
	}
}
