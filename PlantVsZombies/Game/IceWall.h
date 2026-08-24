#pragma once

#include "GameObject.h"

class Board;

/**
 * @brief 冰墙工程师留下的全场唯一独立建筑；阻挡同行直射弹并在回暖期持续融化。
 * @details GameObjectManager 持有所有权，Board 仅保留弱引用以提供 O(1) 查询和存档定位。
 */
class IceWall final : public GameObject {
public:
	static constexpr int kDefaultHealth = 1800;
	static constexpr float kBlockHalfWidth = 34.0f;

	IceWall(Board* board, int row, float centerX,
		int health = kDefaultHealth, int maxHealth = kDefaultHealth,
		float thawDamageRemainder = 0.0f);

	void Update() override;
	void Draw(Graphics* g) override;
	int GetSortingKey() const override { return mRow; }

	int GetRow() const { return mRow; }
	float GetCenterX() const;
	int GetHealth() const { return mHealth; }
	int GetMaxHealth() const { return mMaxHealth; }
	float GetThawDamageRemainder() const { return mThawDamageRemainder; }
	bool IntersectsHorizontalSegment(float fromX, float toX) const;
	Vector GetProjectileAimPosition() const;
	/** 平射弹结算入口；火豆在墙侧集中应用两倍倍率。 */
	int TakeProjectileDamage(int damage, bool fireDamage);
	/** 盐晶独立腐蚀入口；只消费现存墙体生命且不向其他目标溢出。 */
	int ApplyWinterCorrosion(int corrosion);
	/** AutoTest 专用：直接设置墙体位置和生命，不播放命中反馈。 */
	void SetStateForTesting(float centerX, int health, float thawDamageRemainder = 0.0f);

private:
	int ApplyDamage(int damage, bool emitHitFeedback);
	void Break();
	float FindPlantStopCenterX() const;
	const std::string& GetTextureKey() const;

	Board* mBoard = nullptr;
	int mRow = 0;
	int mHealth = kDefaultHealth;
	int mMaxHealth = kDefaultHealth;
	float mThawDamageRemainder = 0.0f;
};
