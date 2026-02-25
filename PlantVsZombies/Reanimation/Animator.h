#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include "../Graphics.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include <iostream>

int ColorComponentMultiply(int theColor1, int theColor2);
SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2);

struct AnimDrawCommand {
    const GLTexture* texture;
    BlendMode blendMode;
    glm::vec4 color;
    float points[8];  // 4涓《鐐圭殑涓栫晫鍧愭爣 (x1,y1, x2,y2, x3,y3, x4,y4)
};

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;
    float mFPS = 12.0f;
    float mFrameIndexNow = 0.0f;
    float mFrameIndexBegin = 0.0f;
    float mFrameIndexEnd = 0.0f;
    float mSpeed = 1.0f;
    float mAlpha = 1.0f;

    // 杩囨浮鍔ㄧ敾
    float mReanimBlendCounter = -1.0f;
    float mReanimBlendCounterMax = 100.0f;
    int mFrameIndexBlendBuffer = 0;

    bool mIsPlaying = false;
    PlayState mPlayingState = PlayState::PLAY_NONE;
    std::vector<TrackExtraInfo> mExtraInfos;
    std::unordered_map<std::string, int> mTrackIndicesMap;

    bool mEnableExtraAdditiveDraw = false;  // 楂樹寒鏁堟灉 
    bool mEnableExtraOverlayDraw = false;   // 闄勫姞瑕嗙洊鏁堟灉
    SDL_Color mExtraAdditiveColor = { 255, 255, 255, 128 };
    SDL_Color mExtraOverlayColor = { 255, 255, 255, 64 };

    // 杩囨浮鐩爣
    std::string mTargetTrack = "";
    float mOriginalSpeed = 1.0f;    // 鍘熸潵鐨勯€熷害

    std::unordered_multimap<int, std::function<void()>> mFrameEvents;  // 甯т簨浠惰〃

public:
    Animator();
    Animator(std::shared_ptr<Reanimation> reanim);
    ~Animator();

    // 鍒濆鍖?
    void Init(std::shared_ptr<Reanimation> reanim);

    void Die();

    // 鎾斁鎺у埗
    void Play(PlayState state = PlayState::PLAY_REPEAT);
    void Pause();
    void Stop();

    // 澧炲姞甯т簨浠?
    void AddFrameEvent(int frameIndex, std::function<void()> callback);

    // 鎾斁鎸囧畾杞ㄩ亾鍔ㄧ敾锛屾敮鎸佽繃娓℃晥鏋?
    bool PlayTrack(const std::string& trackName, float speed = 1.0f, float blendTime = 0);

    // 鎾斁鎸囧畾杞ㄩ亾鍔ㄧ敾涓€娆★紝鎾斁瀹屽悗鍙垏鎹㈠洖鎸囧畾杞ㄩ亾
    bool PlayTrackOnce(const std::string& trackName,
        const std::string& returnTrack = "",
        float speed = 1.0f,
        float blendTime = 0);

    void SetOriginalSpeed(float speed) {
        this->mOriginalSpeed = speed;
    }

    float GetOriginalSpeed()
    {
        return this->mOriginalSpeed;
    }

    // 杞ㄩ亾鑼冨洿鎺у埗
    std::pair<int, int> GetTrackRange(const std::string& trackName);
    void SetFrameRange(int frameBegin, int frameEnd);
    void SetFrameRangeByTrackName(const std::string& trackName);
    void SetFrameRangeToDefault();

    // 璁剧疆杞ㄩ亾鑷畾涔夌汗鐞嗭紙浼氳鐩栧姩鐢诲師鏈殑绾圭悊锛?
    void SetTrackImage(const std::string& trackName, const GLTexture* image);
    void SetTrackVisible(const std::string& trackName, bool visible); // 杞ㄩ亾鏄剧ず鎺у埗
 
    // 灏嗗瓙鍔ㄧ敾闄勫姞鍒版寚瀹氳建閬擄紙璺熼殢杞ㄩ亾鍙樻崲锛?
    bool AttachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);
    void DetachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);
    void DetachAllAnimators();

    // 鐘舵€佹煡璇?
    bool IsPlaying() const { return mIsPlaying; }
    float GetCurrentFrame() const { return mFrameIndexNow; }
    void SetSpeed(float speed);
    float GetSpeed() const { return mSpeed; }

    // 鑾峰彇Track鐨勫姩閲?
    float GetTrackVelocity(const std::string& trackName) const;

    // 閫忔槑搴﹀拰棰滆壊鎺у埗
    void SetAlpha(float alpha);
    float GetAlpha() const { return mAlpha; }

    void EnableGlowEffect(bool enable);
    void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128);

    void EnableOverlayEffect(bool enable);
    void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64);

    // 鏇存柊鍜屾覆鏌?
    void Update();
    void Draw(Graphics* g, float baseX, float baseY, float Scale = 1.0f);

    // 澶氱嚎绋嬫敮鎸侊細棰勮绠楀彉鎹紝缁撴灉缂撳瓨鍦?mCachedCommands 涓?
    // 鍙粠浠绘剰绾跨▼璋冪敤锛堢函CPU璁＄畻锛屼笉娑夊強OpenGL锛?
    // Draw() 浼氳嚜鍔ㄤ娇鐢ㄧ紦瀛橈紝鐢ㄥ畬鍚庢竻绌?
    void PrepareCommands(float baseX, float baseY, float Scale = 1.0f);

    // 鑾峰彇搴曞眰Reanimation
    std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

    // 鑾峰彇杞ㄩ亾淇℃伅
    std::vector<TrackInfo*> GetTracksByName(const std::string& trackName) const;
    Vector GetTrackPosition(const std::string& trackName) const;
    float GetTrackRotation(const std::string& trackName) const;
    bool GetTrackVisible(const std::string& trackName) const;

    void SetLocalPosition(float x, float y);
    void SetLocalPosition(const Vector& position);
    void SetLocalScale(float sx, float sy);
    void SetLocalRotation(float rotation);

private:
    // 瀛愬姩鐢荤浉瀵逛簬鐖惰建閬撶殑鏈湴鍙樻崲锛堝崟浣嶏細鍍忕礌銆佺缉鏀惧洜瀛愩€佽搴︼級
    float mLocalPosX = 0.0f;
    float mLocalPosY = 0.0f;
    float mLocalScaleX = 1.0f;
    float mLocalScaleY = 1.0f;
    float mLocalRotation = 0.0f;   // 瑙掑害鍒?

    std::vector<AnimDrawCommand> mCachedCommands;  // PrepareCommands() 鐨勭紦瀛樼粨鏋?

private:
    TrackFrameTransform GetInterpolatedTransform(int trackIndex) const;
    std::vector<TrackExtraInfo*> GetTrackExtrasByName(const std::string& trackName);
    int GetFirstTrackIndexByName(const std::string& trackName) const;
    void CollectDrawCommands(std::vector<AnimDrawCommand>& outCommands, float baseX, float baseY, float Scale) const;
};

#endif