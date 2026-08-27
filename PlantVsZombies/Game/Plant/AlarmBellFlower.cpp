#include "AlarmBellFlower.h"

#include "../../DeltaTime.h"
#include "../../GameApp.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <algorithm>
#include <limits>

namespace {
constexpr float kAfterglowDuration = 1.0f; // 脉冲提交后实体保留的演出游戏秒数
constexpr float kRingClipSpeed = 2.0f; // 原版 Blover anim_blow 的警铃草播放倍率
constexpr float kFinalFadeOutDuration =
    0.22f; // 消失前切疲惫表情并整体淡出的秒数
constexpr float kRemainingTieEpsilon =
    0.0001f;                               // 余时视为并列的浮点容差，单位秒
constexpr float kBellSoundVolume = 0.34f;  // 电子提示音音量，提供清晰动作边沿
constexpr float kMetalSoundVolume = 0.22f; // 金属撞击叠音量，强化实体铃铛质感
constexpr const char *kCharmTrack =
    "Blover_stem1"; // 小铃挂在稳定上段茎，保留三叶草头和叶片的完整原版动作
constexpr const char *kCharmFollowerSlot =
    "alarm_bell_charm"; // AI 铃铛只作为低分辨率身份挂件，不替换主体分件
constexpr float kCharmOffsetX = 11.0f; // 小铃相对上段茎右侧的局部 X 偏移，单位 px
constexpr float kCharmOffsetY = 8.0f;  // 小铃垂挂在原版头部下缘，单位 px

/** 返回区间内平滑启停的 0～1 权重，用于整株退出淡出。 */
float SmoothStep(float start, float end, float value) {
  const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

} // namespace

void AlarmBellFlower::SetupPlant() {
  ConfigureRig();
  if (auto *shadow = GetShadow()) {
    shadow->SetOffset(Vector(0.0f, 27.0f));
    shadow->SetScale(Vector(0.82f, 0.72f));
  }
  PlayTrack("anim_idle", 1.0f);
  RefreshPresentation();
}

void AlarmBellFlower::PlantUpdate() {
  if (mIsPreview || !mBoard)
    return;
  if (!mPulseTriggered) {
    TriggerPulse();
    return;
  }

  mAfterglowRemaining =
      std::max(0.0f, mAfterglowRemaining - DeltaTime::GetDeltaTime());
  RefreshPresentation();
  if (mAfterglowRemaining <= 0.0f)
    Die();
}

void AlarmBellFlower::TakeDamage(int, DamageSource) {
  // 即时结算完成后只保留短演出，不让啃食或范围攻击制造第二种取消语义。
  SetGlowingTimer(0.1f);
}

void AlarmBellFlower::SaveExtraData(nlohmann::json &j) const {
  j["pulseTriggered"] = mPulseTriggered;
  j["pulseSucceeded"] = mPulseSucceeded;
  j["afterglowRemaining"] = mAfterglowRemaining;
}

void AlarmBellFlower::LoadExtraData(const nlohmann::json &j) {
  mPulseTriggered = j.value("pulseTriggered", false);
  mPulseSucceeded = j.value("pulseSucceeded", false);
  mAfterglowRemaining =
      std::clamp(j.value("afterglowRemaining", 0.0f), 0.0f, kAfterglowDuration);
  if (!mPulseTriggered) {
    mPulseSucceeded = false;
    mAfterglowRemaining = 0.0f;
  } else if (mAfterglowRemaining <= 0.0f) {
    // 损坏快照不能让已经提交的即时植物重新触发；下一逻辑步只负责回收。
    mAfterglowRemaining = 0.0f;
  }
  RefreshPresentation();
}

Zombie *AlarmBellFlower::FindInterruptTarget() const {
  if (!mBoard)
    return nullptr;
  Zombie *selected = nullptr;
  float selectedRemaining = std::numeric_limits<float>::max();
  mBoard->mEntityRegistry.ForEachZombieInRow(mRow, [&](Zombie *zombie) {
    if (!zombie || !zombie->IsActive() || zombie->IsDying() ||
        zombie->IsMindControlled())
      return;
    const float remaining = zombie->GetInterruptibleSpecialActionRemaining();
    if (!std::isfinite(remaining) || remaining < 0.0f)
      return;
    const bool shorter = remaining < selectedRemaining - kRemainingTieEpsilon;
    const bool tied =
        std::abs(remaining - selectedRemaining) <= kRemainingTieEpsilon;
    if (shorter ||
        (tied && (!selected || zombie->mZombieID < selected->mZombieID))) {
      selected = zombie;
      selectedRemaining = remaining;
    }
  });
  return selected;
}

void AlarmBellFlower::TriggerPulse() {
  if (mPulseTriggered || !mBoard)
    return;
  mPulseTriggered = true;
  mAfterglowRemaining = kAfterglowDuration;
  if (Zombie *target = FindInterruptTarget()) {
    // 候选只选择一次；动作若在调用边沿失效，不改投下一名僵尸。
    mPulseSucceeded = target->InterruptUncommittedSpecialAction();
  }

  // 直接复用原版三叶草 anim_blow 的 33..51 帧弯折与回弹，再进入 52..61
  // 的余韵段。
  PlayTrackOnce("anim_blow", "anim_loop", kRingClipSpeed, 0.0f, kRingClipSpeed,
                0.0f);

  AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_BLEEP, kBellSoundVolume);
  AudioSystem::PlaySound(ResourceKeys::Sounds::SOUND_IRONHIT2,
                         kMetalSoundVolume);
  if (g_particleSystem) {
    const float rowCenterX =
        CELL_INITALIZE_POS_X +
        static_cast<float>(mBoard->mColumns) * CELL_COLLIDER_SIZE_X * 0.5f;
    const Vector pulseAnchor(rowCenterX,
                             mBoard->GetRowCenterYAtX(mRow, rowCenterX) - 8.0f);
    g_particleSystem->EmitEffect("AlarmBellRowPulse", pulseAnchor);
  }
  RefreshPresentation();
}

void AlarmBellFlower::ConfigureRig() {
  if (!mAnimator)
    return;
  // 主体完全由派生换色的 Blover.reanim 负责；小铃只跟随稳定上段茎。
  mAnimator->SetTrackFollowerImage(
      kCharmTrack, kCharmFollowerSlot,
      ResourceManager::GetInstance().GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_CHARM),
      kCharmOffsetX, kCharmOffsetY, 1.0f, 1.0f, false, true);
  mAnimator->SetTrackFollowerVisible(kCharmTrack, kCharmFollowerSlot, true);
}

void AlarmBellFlower::RefreshPresentation() {
  if (!mAnimator)
    return;
  const float opacity =
      mPulseTriggered
          ? SmoothStep(0.0f, kFinalFadeOutDuration, mAfterglowRemaining)
          : 1.0f;
  // 整体 Alpha 同时覆盖原版主体与命名 follower，两条渲染路径保持一致。
  SetAlpha(opacity);
}
