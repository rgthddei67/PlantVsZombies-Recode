#pragma once

#include "Plant.h"

/**
 * 警铃草：种下后中断本行剩余提交时间最短的一项特殊动作，再播放短暂响铃余韵。
 */
class AlarmBellFlower final : public Plant {
public:
  using Plant::Plant;
  /** 已进入结算动作时禁止搬运，保留原目标及动作提交位置。 */
  bool CanBeRelocated() const override { return false; }

  void PlantUpdate() override;
  bool CanBeEaten() const override { return false; }
  void TakeDamage(int damage, DamageSource source) override;
  /** 保存单次脉冲提交与演出余时，读档不得重复打断。 */
  void SaveExtraData(nlohmann::json &j) const override;
  /** 恢复演出阶段；旧档缺字段时按尚未响铃处理。 */
  void LoadExtraData(const nlohmann::json &j) override;

  bool HasTriggeredPulse() const { return mPulseTriggered; }
  bool DidInterruptAction() const { return mPulseSucceeded; }
  float GetAfterglowRemaining() const { return mAfterglowRemaining; }

protected:
  /** 设置待命立绘与影子；正式结算由首个逻辑步完成，不注册新帧事件。 */
  void SetupPlant() override;

private:
  /** 在本行按剩余提交时间和稳定实体 ID 选择唯一候选。 */
  class Zombie *FindInterruptTarget() const;
  /** 原子提交一次中断，并启动声音、行脉冲和响铃演出。 */
  void TriggerPulse();
  /** 保留完整三叶草时间轴，并在稳定上段茎挂接小铃身份件。 */
  void ConfigureRig();
  /** 按余韵时间同步原版主体与小铃挂件的整体淡出。 */
  void RefreshPresentation();

  bool mPulseTriggered = false;
  bool mPulseSucceeded = false;
  float mAfterglowRemaining = 0.0f;
};
