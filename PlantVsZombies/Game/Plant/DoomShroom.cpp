#include "DoomShroom.h"
#include "../Crater.h"
#include "../AudioSystem.h"
#include "../ShadowComponent.h"

void DoomShroom::SetupPlant()
{
	auto shadow = GetComponent<ShadowComponent>();
	shadow->SetScale(Vector(1.0f, 1.0f));   // 宽扁大伞盖，比小蘑菇系(0.6)大一档（主人校对）
	shadow->SetOffset(Vector(2, 30));       // 较其他蘑菇右移 6px（主人校对：本体略偏右）

	Shroom::SetupPlant();

	// 预览/图鉴绝不结算（图鉴也会走到这里）
	if (mIsPreview) return;

	// 全局第 51 帧 = anim_explode(19..51) 末帧引爆（主人指定，帧号已按代码口径-1）。
	// 读档时 SetupPlant 重新注册：RestoreAnimState 恢复的帧在 51 之前，穿过时照常触发。
	mAnimator->AddFrameEvent(51, [this]() {
		Explode();
		});

	// 夜晚种下立即充能：镜像原版 UpdateDoomShroom（23fps 播 anim_explode + 吸气充能声）；
	// 白天睡觉由 Shroom 基类切 anim_sleep，充能推迟到永远（本项目关卡昼夜固定，无咖啡豆）
	if (!mIsSleeping) {
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_REVERSE_EXPLOSION, 0.5f);
		this->PlayTrack("anim_explode", 23.0f / 12.0f, 0);
	}
}

void DoomShroom::TakeDamage(int damage, DamageSource source)
{
	// 充能期间无敌（参考樱桃炸弹：只闪光不掉血）；白天睡觉=普通蘑菇，照常被啃
	if (!mIsSleeping) {
		this->SetGlowingTimer(0.1f);
		return;
	}
	Plant::TakeDamage(damage, source);
}

void DoomShroom::Explode()
{
	if (!mBoard) return;
	// 音效 + Doom 粒子（蘑菇云/DOOM 字样/紫屏闪）+ 半径 250 圆形结算都在 CreateDoomBoom 内
	mBoard->CreateDoomBoom(GetPosition());
	mBoard->AddCrater(mRow, mColumn, Crater::CRATER_DURATION);
	Die();
}
