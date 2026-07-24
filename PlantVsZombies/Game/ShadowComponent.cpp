#include "ShadowComponent.h"
#include "../Logger.h"
#include "../ResourceManager.h"
#include "GameObject.h"
#include <algorithm>
#include "Plant/Plant.h"
#include "Zombie/Zombie.h"

ShadowComponent::ShadowComponent(const Texture* shadowTexture,
	const Vector& offset,
	float alpha)
	: mShadowTexture(shadowTexture)
	, mOffset(offset)
	, mAlpha(alpha) {
}

ShadowComponent::~ShadowComponent() {
	mShadowTexture = nullptr;
}

void ShadowComponent::Start() {
	if (!mShadowTexture) {
		LOG_DEBUG("ShadowComponent") << "No shadow texture set, using default texture.";
		mShadowTexture = ResourceManager::GetInstance().GetTexture(
			ResourceKeys::Textures::IMAGE_PLANTSHADOW);
	}
}

void ShadowComponent::Draw(Graphics* g) {
	if (!mVisible) return;

	auto gameObject = GetGameObject();
	if (!gameObject) return;

	// 计算阴影位置（在物体下方，加上偏移）
	ObjectType type = gameObject->GetObjectType();
	Vector shadowPos = Vector(0, 0);
	auto transform = gameObject->GetComponent<TransformComponent>();
	if (!transform) {
		LOG_ERROR("ShadowComponent") << "GameObject has no TransformComponent.";
		return;
	}
	Vector objPos = transform->GetPosition();
	if (type == ObjectType::OBJECT_PLANT) {
		// 阵风换格时碰撞箱已在目标格，植物本体与阴影共同使用纯视觉偏移追赶。
		if (auto* plant = dynamic_cast<Plant*>(gameObject)) {
			objPos = plant->GetGridVisualPosition();
		}
	}

	if (type == ObjectType::OBJECT_PLANT || type == ObjectType::OBJECT_ZOMBIE
		|| type == ObjectType::OBJECT_LAWNMOWER || type == ObjectType::OBJECT_BULLET) {
		shadowPos = objPos + mOffset;
	}
	else {
		LOG_WARN("ShadowComponent") << "GameObject is not Plant, Zombie, Mower or Bullet.";
		return;
	}

	if (!mShadowTexture) return;

	// 纹理尺寸
	float texWidth = static_cast<float>(mShadowTexture->width);
	float texHeight = static_cast<float>(mShadowTexture->height);

	// 计算绘制位置（以阴影位置为中心）
	float drawX = shadowPos.x - texWidth * mScale.x * 0.5f;
	float drawY = shadowPos.y - texHeight * mScale.y * 0.5f;
	float drawW = texWidth * mScale.x;
	float drawH = texHeight * mScale.y;

	// 绘制阴影，使用 tint 的 alpha 控制透明度
	glm::vec4 tint(255.0f, 255.0f, 255.0f, mAlpha * 255.0f);
	if (g->IsInstancePathEnabled()) {
		// 影子与 reanim 本体必须留在同一实例队列。否则并行 replay 会把同段所有 batch
		// 提前到 instance 之前，水路上层植物的影子便会被先前绘制的睡莲本体反盖。
		g->DrawTextureInstanced(mShadowTexture, drawX, drawY, drawW, drawH, 0.0f, tint);
	}
	else {
		// -NoInstance 继续使用原普通批次路径，保持故障兜底与视觉 A/B 能力。
		g->DrawTexture(mShadowTexture, drawX, drawY, drawW, drawH, 0.0f, tint);
	}
}
