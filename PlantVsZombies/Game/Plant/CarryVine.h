#pragma once

#include "Plant.h"

/** 搬搬藤只承担工具卡预览与瞬时搬运反馈，正式事务由 Board 原子提交。 */
class CarryVine final : public Plant {
public:
	using Plant::Plant;
	bool CanBeRelocated() const override { return false; }
	bool CanBeEaten() const override { return false; }
	bool OccupiesGridSlot() const override { return false; }
	/** 非战斗预览实例播放短余韵；不保存也不重复执行搬运。 */
	void BeginFeedback(const Vector& position);
	void Update() override;
private:
	float mFeedbackRemaining = -1.0f; // 小于零表示普通预览，否则为纯演出剩余游戏秒
protected:
	/** 预览使用独立资源中完整藤臂骨架与叶篮，不建立战斗碰撞。 */
	void SetupPlant() override;
};
