#pragma once
#ifndef _SHADOW_COMPONENT_H
#define _SHADOW_COMPONENT_H

#include "Transform.h"
#include "Definit.h"
#include <algorithm>

class GameObject;
class Graphics;
struct Texture;

/**
 * @brief GameObject 显式拥有的可选阴影附件。
 * @details 宿主负责创建、固定阶段绘制和销毁，不经过额外的通用生命周期。
 */
class ShadowComponent {
private:
	GameObject* mGameObject = nullptr; // 宿主独占本对象，生命周期严格短于宿主
	const Texture* mShadowTexture = nullptr;
	Vector mOffset = Vector(15, 28);  // 阴影相对于物体位置的偏移
	Vector mScale = Vector(1.0f, 0.75f);  // 阴影的缩放（通常水平拉伸，垂直压缩）
	float mAlpha = 0.9f;            // 阴影透明度
	mutable bool mEnabled = true;   // 动作阶段门控；与介质/出土等 mVisible 原因独立并取 AND
	mutable bool mVisible = true;   // 纯展示派生状态；const 宿主同步介质/阶段时也可更新
	Vector mLastDrawCenter = Vector::zero(); // 最近一次实际提交的阴影中心，供 AutoTest 取证
	bool mLastDrawReady = false;    // 本帧是否已提交有效阴影几何

	friend class GameObject;
	ShadowComponent(GameObject* gameObject,
		const Texture* shadowTexture,
		const Vector& offset,
		float alpha);

public:
	~ShadowComponent() = default;

	/** 在宿主的固定阴影阶段提交绘制；BulletPool 也复用此入口。 */
	void Draw(Graphics* g);

	// 设置阴影纹理
	void SetShadowTexture(const Texture* texture) { mShadowTexture = texture; }

	// 设置阴影偏移
	void SetOffset(const Vector& offset) { mOffset = offset; }

	// 设置阴影透明度
	void SetAlpha(float alpha) { mAlpha = std::clamp(alpha, 0.0f, 1.0f); }

	// 设置阴影缩放
	void SetScale(const Vector& scale) { mScale = scale; }
	Vector GetScale() const { return mScale; }

	// 设置阴影可见性（false 时跳过绘制，附件仍在）
	void SetVisible(bool visible) const { mVisible = visible; }
	bool IsVisible() const { return mVisible; }
	/** 设置动作阶段门控；与 SetVisible 的介质/生命周期门控互不覆盖。 */
	void SetEnabled(bool enabled) const { mEnabled = enabled; }
	bool IsEnabled() const { return mEnabled; }

	/** 返回配置的阴影中心偏移，供最终绘制坐标取证。 */
	Vector GetOffset() const { return mOffset; }
	/** 最近一次 Draw 是否实际提交了阴影。 */
	bool IsLastDrawReady() const { return mLastDrawReady; }
	/** 返回最近一次实际提交的阴影中心。 */
	Vector GetLastDrawCenter() const { return mLastDrawCenter; }
};

#endif
