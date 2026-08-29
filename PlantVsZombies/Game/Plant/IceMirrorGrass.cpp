#include "IceMirrorGrass.h"

#include "../../DeltaTime.h"
#include "../../GameApp.h"
#include "../../ResourceKeys.h"
#include "../AudioSystem.h"
#include "../ShadowComponent.h"

#include <algorithm>

namespace {
constexpr int kMaximumMirrors = 2; // 单株同时维持的镜片上限
constexpr float kMirrorFormationSeconds = 12.0f; // 每凝结一面镜片所需的游戏秒
constexpr float kMirrorOffsetX = 22.0f; // 前后镜片相对植物视觉原点的横向距离，单位 px
constexpr float kMirrorOffsetY = -24.0f; // 镜片相对植物视觉原点的纵向距离，单位 px
}

void IceMirrorGrass::SetupPlant()
{
	mPlantHealth = 300;
	mPlantMaxHealth = 300;
	if (auto* shadow = GetShadow()) {
		shadow->SetOffset(Vector(0.0f, 29.0f));
		shadow->SetScale(Vector(0.78f, 0.55f));
	}
}

void IceMirrorGrass::PlantUpdate()
{
	if (mIsPreview || mMirrorCount >= kMaximumMirrors) return;
	mFormationProgress += DeltaTime::GetDeltaTime();
	while (mFormationProgress >= kMirrorFormationSeconds
		&& mMirrorCount < kMaximumMirrors) {
		mFormationProgress -= kMirrorFormationSeconds;
		++mMirrorCount;
		AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CERAMIC, 0.22f);
	}
	if (mMirrorCount >= kMaximumMirrors) mFormationProgress = 0.0f;
}

bool IceMirrorGrass::TryInterceptHostileStraightProjectile(
	float velocityX, const Vector& impactPosition)
{
	if (mMirrorCount <= 0 || !IsActive() || mIsPreview || IsSquished()) return false;
	--mMirrorCount;
	AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_CERAMIC, 0.42f);
	if (g_particleSystem) {
		g_particleSystem->EmitEffect("IceMirrorBreak",
			impactPosition + Vector(velocityX < 0.0f ? 12.0f : -12.0f, 0.0f));
	}
	return true;
}

void IceMirrorGrass::Draw(Graphics* g)
{
	Plant::Draw(g);
	if (!g) return;
	const Vector center = GetVisualAnchorPosition()
		+ Vector(0.0f, kMirrorOffsetY);
	for (int i = 0; i < mMirrorCount; ++i) {
		const float direction = i == 0 ? 1.0f : -1.0f;
		const float x = center.x + direction * kMirrorOffsetX;
		g->FillCircle(x, center.y, 15.0f,
			glm::vec4(126.0f, 224.0f, 255.0f, 75.0f), 24);
		g->DrawCircle(x, center.y, 15.0f,
			glm::vec4(221.0f, 252.0f, 255.0f, 225.0f), 24);
		g->DrawLine(x - 7.0f, center.y + 8.0f, x + 8.0f, center.y - 9.0f,
			glm::vec4(255.0f, 255.0f, 255.0f, 180.0f));
	}
}

void IceMirrorGrass::SaveExtraData(nlohmann::json& j) const
{
	j["mirrorCount"] = mMirrorCount;
	j["formationProgress"] = mFormationProgress;
}

void IceMirrorGrass::LoadExtraData(const nlohmann::json& j)
{
	mMirrorCount = std::clamp(j.value("mirrorCount", 0), 0, kMaximumMirrors);
	mFormationProgress = mMirrorCount >= kMaximumMirrors ? 0.0f
		: std::clamp(j.value("formationProgress", 0.0f),
			0.0f, kMirrorFormationSeconds);
}
