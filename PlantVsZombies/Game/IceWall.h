#pragma once

#include "GameObject.h"
#include "Zombie/ZombieType.h"

class Board;

/**
 * @brief 冰墙工程师部署的全场唯一独立建筑；施工期掩护建造者，完工后持续向房屋推进。
 * @details GameObjectManager 持有所有权，Board 仅保留弱引用以提供 O(1) 查询和存档定位。
 */
class IceWall final : public GameObject {
public:
	static constexpr int kDefaultHealth = 1800; // 完工墙体的当前与最大生命
	static constexpr int kConstructionHealth = 600; // 半成品墙体的初始生命
	static constexpr float kBlockHalfWidth = 34.0f; // 墙体平射阻挡半宽，单位 px

	IceWall(Board* board, int row, float centerX,
		int health = kDefaultHealth, int maxHealth = kDefaultHealth,
		float thawDamageRemainder = 0.0f, bool constructionComplete = true,
		int builderZombieID = NULL_ZOMBIE_ID);

	void Update() override;
	void Draw(Graphics* g) override;
	int GetSortingKey() const override { return mRow; }

	int GetRow() const { return mRow; }
	float GetCenterX() const;
	int GetHealth() const { return mHealth; }
	int GetMaxHealth() const { return mMaxHealth; }
	float GetThawDamageRemainder() const { return mThawDamageRemainder; }
	bool IsConstructionComplete() const { return mConstructionComplete; }
	int GetBuilderZombieID() const { return mBuilderZombieID; }
	bool IsUnderConstructionBy(int zombieID) const;
	bool IntersectsHorizontalSegment(float fromX, float toX) const;
	Vector GetProjectileAimPosition() const;
	/** 仅匹配的施工者可把未完成墙硬化为满生命完整墙。 */
	bool CompleteConstruction(int builderZombieID);
	/** 仅匹配的施工者可取消未完成墙；取消会走正式碎裂反馈。 */
	bool AbortConstruction(int builderZombieID);
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
	bool mConstructionComplete = true;
	int mBuilderZombieID = NULL_ZOMBIE_ID;
};
