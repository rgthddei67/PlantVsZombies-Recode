#pragma once

#include "Shooter.h"

class ThreePeater : public Shooter {
public:
	using Shooter::Shooter;

	const Animator* GetHeadAnimator2() const { return mHeadAnim2.get(); }
	const Animator* GetHeadAnimator3() const { return mHeadAnim3.get(); }

	/** 保存攻击计时器及三个独立头部 Animator 的完整播放状态。 */
	void SaveExtraData(nlohmann::json& j) const override;
	/** 恢复三个头部的播放状态，保证射击中读档仍只完成当前一轮。 */
	void LoadExtraData(const nlohmann::json& j) override;

	void PlantUpdate() override;

protected:
	/** 创建三个附着头部，并按主人确认的全局帧注册发射事件。 */
	void SetupPlant() override;
	void ShootBullet() override;

private:
	std::shared_ptr<Animator> mHeadAnim2;
	std::shared_ptr<Animator> mHeadAnim3;

	std::shared_ptr<Animator> CreateHeadAnimator(
		const char* idleTrack, const char* attachTrack,
		float basePoseX, float basePoseY, int fireFrame, int targetRowOffset);
	/** 判断指定有效行前方是否存在可攻击僵尸。 */
	bool HasTargetInRow(int row) const;
	/** 按 C# 规则检查本行及相邻两行，只需任一行命中即可启动整轮。 */
	bool HasTargetInThreeRows() const;
	bool IsValidRow(int row) const;
	/** 同时启动三个头的一次性射击动画，边界头也参与补偿射击。 */
	void StartVolley(float attackSpeedMultiplier);
	/** 从统一枪口创建豌豆；越界目标补发到本行，相邻行启用斜向轨迹。 */
	void FireRow(int targetRow);
};
