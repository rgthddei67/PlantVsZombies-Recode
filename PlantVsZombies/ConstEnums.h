#pragma once

#ifndef _CONSTENUMS_H
#define _CONSTENUMS_H

#include <string>

// ��������
enum class ReanimationType 
{
    REANIM_NONE,
    REANIM_LOADBAR_SPROUT,
    REANIM_SUNFLOWER,
    REANIM_PEASHOOTER,
    REANIM_ZOMBIE,
    REANIM_SUN,
    NUM_REANIMS
};

// ����ѭ������
enum class ReanimLoopType {
    REANIM_PLAY_ONCE,
    REANIM_LOOP,
    REANIM_PLAY_ONCE_AND_HOLD,
    REANIM_LOOP_FULL_LAST_FRAME,
    REANIM_PLAY_ONCE_FULL_LAST_FRAME,
    REANIM_PLAY_ONCE_FULL_LAST_FRAME_AND_HOLD
};

// Ч������
enum class EffectType {
    EFFECT_NONE,
    EFFECT_PARTICLE,
    EFFECT_TRAIL,
    EFFECT_REANIM,
    EFFECT_ATTACHMENT
};

// ����ID����
enum class AttachmentID {
    ATTACHMENTID_NULL = 0
};

// ��Ⱦ����
constexpr int RENDER_GROUP_HIDDEN = -1;
constexpr int RENDER_GROUP_NORMAL = 0;

#endif