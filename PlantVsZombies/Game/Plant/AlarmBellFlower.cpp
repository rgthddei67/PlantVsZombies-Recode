#include "AlarmBellFlower.h"

#include "../../DeltaTime.h"
#include "../../GameApp.h"
#include "../../ResourceKeys.h"
#include "../../ResourceManager.h"
#include "../Board.h"
#include "../ShadowComponent.h"
#include "../Zombie/Zombie.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
constexpr float kAfterglowDuration = 1.0f; // 脉冲提交后实体保留的演出游戏秒数
constexpr float kRingClipSpeed = 2.0f; // 原版 Blover anim_blow 的警铃草播放倍率
constexpr float kFinalFadeOutDuration =
    0.22f; // 消失前切疲惫表情并整体淡出的秒数
constexpr float kRingingHeadFrame =
    43.0f; // Blover HEAD→HEAD2 的原版表情切换全局帧
constexpr float kRemainingTieEpsilon =
    0.0001f;                               // 余时视为并列的浮点容差，单位秒
constexpr float kBellSoundVolume = 0.34f;  // 电子提示音音量，提供清晰动作边沿
constexpr float kMetalSoundVolume = 0.22f; // 金属撞击叠音量，强化实体铃铛质感
constexpr const char *kBaseTrack =
    "Blover_dirt_back"; // 原版地面轨承载茎叶底座的轻微受力
constexpr const char *kLowerStemTrack =
    "Blover_stem2"; // 原版下段茎连接固定叶座与摆动上身
constexpr const char *kUpperStemTrack =
    "Blover_stem1"; // 原版上段茎把铃身挂到下段茎的末端
constexpr const char *kHeadTrack =
    "Blover_head"; // 原版头部位移/缩放轨承载铃铛头

/** 返回区间内平滑启停的 0～1 权重，用于整株退出淡出。 */
float SmoothStep(float start, float end, float value) {
  const float t = std::clamp((value - start) / (end - start), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

SDL_Color OpacityColor(float opacity) {
  const auto alpha =
      static_cast<Uint8>(std::lround(std::clamp(opacity, 0.0f, 1.0f) * 255.0f));
  return SDL_Color{255, 255, 255, alpha};
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
  static constexpr const char *kHiddenTracks[] = {
      "Blover_dirt_front",
      "Blover_petals",
      "Blover_petals2",
      "Blover_petals3",
  };
  for (const char *track : kHiddenTracks)
    mAnimator->SetTrackVisible(track, false);

  auto &resources = ResourceManager::GetInstance();
  mAnimator->SetTrackImage(
      kBaseTrack,
      resources.GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_BASE));
  mAnimator->SetTrackImage(
      kHeadTrack,
      resources.GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_HEAD_READY));
  // 铃舌与铃身共用父轨完整仿射变换，摆到极限时也不会脱离铃口。
  mAnimator->SetTrackFollowerImage(
      kHeadTrack,
      resources.GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_CLAPPER),
      0.0f, 0.0f, 1.0f, 1.0f, false);
  // 两张 120px 分件图共用中心轴；常量抵消 Blover 各轨道首帧的原版绝对位置。
  mAnimator->SetTrackOffset(kBaseTrack, -29.4f, -58.3f);
  mAnimator->SetTrackOffset(kHeadTrack, -13.9f, -8.3f);
}

void AlarmBellFlower::RefreshPresentation() {
  if (!mAnimator)
    return;
  const Texture *head = ResourceManager::GetInstance().GetTexture(
      ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_HEAD_READY);
  if (mPulseTriggered) {
    if (mAfterglowRemaining <= kFinalFadeOutDuration) {
      head = ResourceManager::GetInstance().GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_HEAD_FADING);
    } else if (mAnimator->GetCurrentTrackName() == "anim_loop" ||
               mAnimator->GetCurrentFrame() >= kRingingHeadFrame) {
      head = ResourceManager::GetInstance().GetTexture(
          ResourceKeys::Textures::IMAGE_REANIM_ALARMBELLFLOWER_HEAD_RINGING);
    }
  }
  mAnimator->SetTrackImage(kHeadTrack, head);

  const float opacity =
      mPulseTriggered
          ? SmoothStep(0.0f, kFinalFadeOutDuration, mAfterglowRemaining)
          : 1.0f;
  const SDL_Color color = OpacityColor(opacity);
  mAnimator->SetTrackColor(kBaseTrack, color);
  mAnimator->SetTrackColor(kLowerStemTrack, color);
  mAnimator->SetTrackColor(kUpperStemTrack, color);
  mAnimator->SetTrackColor(kHeadTrack, color);
}
