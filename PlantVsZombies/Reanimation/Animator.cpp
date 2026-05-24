#include "Animator.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <cmath>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Animator::Animator() {
    mReanim = nullptr;
}

Animator::Animator(std::shared_ptr<Reanimation> reanim) {
    Init(reanim);
}

Animator::~Animator() {
    Die();
}

void Animator::Die() {
    // 先让所有附加的子动画死亡
    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->Die();
            }
        }
        mExtraInfos[i].mAttachedReanims.clear();
    }
    mFrameEvents.clear();
    mIsPlaying = false;
}

void Animator::Init(std::shared_ptr<Reanimation> reanim) {
    mReanim = reanim;
    if (reanim) {
        mFPS = reanim->mFPS;
        mTrackIndicesMap.clear();
        mExtraInfos.clear();
        mFrameEvents.clear();
        for (int i = 0; i < reanim->GetTrackCount(); i++) {
            auto track = reanim->GetTrack(i);
            if (track) {
                mTrackIndicesMap.try_emplace(track->mTrackName, i);
                TrackExtraInfo extra;
                mExtraInfos.push_back(extra);
            }
        }

        mPlayingState = PlayState::PLAY_REPEAT;
        mIsPlaying = false;
        mTargetTrack = "";
    }
}

void Animator::AddFrameEvent(int frameIndex, std::function<void()> callback, bool persistent) {
    if (callback) {
        mFrameEvents.insert({ frameIndex, FrameEvent{ std::move(callback), persistent } });
    }
}

bool Animator::PlayTrack(const std::string& trackName, float speed, float blendTime) {
    auto range = GetTrackRange(trackName);
    if (range.first == -1 || range.second == -1) {
        std::cerr << "动画轨道不存在或为空: " << trackName << std::endl;
        return false;
    }

    // 保存当前帧用于过渡
    mFrameIndexBlendBuffer = static_cast<int>(mFrameIndexNow);

    // 设置新的帧范围
    SetFrameRange(range.first, range.second);
    mFrameIndexNow = static_cast<float>(range.first);

    // 设置过渡效果
    if (blendTime > 0) {
        mReanimBlendCounterMax = blendTime;
        mReanimBlendCounter = blendTime;
    }
    else {
        mReanimBlendCounter = -1.0f;
    }

    if (speed > 0.0f) {
        mOriginalSpeed = mSpeed;
        SetSpeed(speed);
    }

    mIsPlaying = true;
    mPlayingState = PlayState::PLAY_REPEAT;
    mCurrentTrackName = trackName;

    return true;
}

bool Animator::PlayTrackOnce(const std::string& trackName, const std::string& returnTrack,
    float speed, float blendTime) {
    if (!PlayTrack(trackName, speed, blendTime)) {
        return false;
    }

    mPlayingState = PlayState::PLAY_ONCE_TO;
    mTargetTrack = returnTrack;

    return true;
}

void Animator::Play(PlayState state) {
    mPlayingState = state;
    mIsPlaying = true;
}

void Animator::Pause() {
    mIsPlaying = false;
}

void Animator::Stop() {
    mIsPlaying = false;
    mFrameIndexNow = mFrameIndexBegin;
}

void Animator::Update() {
    if (!mIsPlaying || !mReanim) return;

    float deltaTime = DeltaTime::GetDeltaTime();
    float oldFrame = mFrameIndexNow;   // 记录更新前的帧索引

    // 帧索引前进 (mExtraSpeedMultiplier 让状态效果如减速可独立于 mSpeed)
    float frameAdvance = deltaTime * mFPS * mSpeed * mExtraSpeedMultiplier;
    mFrameIndexNow += frameAdvance;

    // 处理动画结束/循环逻辑
    bool reachedEnd = mFrameIndexNow >= mFrameIndexEnd;
    if (reachedEnd) {
        switch (mPlayingState) {
        case PlayState::PLAY_REPEAT:
            mFrameIndexNow = mFrameIndexBegin;
            break;
        case PlayState::PLAY_ONCE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        case PlayState::PLAY_ONCE_TO:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            if (!mTargetTrack.empty()) {
                PlayTrack(mTargetTrack, 0.0f, 0.5f);
                RestoreSpeed();
                mTargetTrack = "";
            }
            break;
        case PlayState::PLAY_NONE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        }
    }

    // 限制帧范围
    mFrameIndexNow = std::clamp(mFrameIndexNow, mFrameIndexBegin, mFrameIndexEnd);

    // ----- 触发帧事件（一次性触发后自动移除；持久事件保留）-----
    int oldInt = static_cast<int>(oldFrame);
    int newInt = static_cast<int>(mFrameIndexNow);

    if (newInt >= oldInt) {
        // 正常前进或不变
        for (int f = oldInt + 1; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }
    else {
        // 发生了回绕（循环播放）
        int endInt = static_cast<int>(mFrameIndexEnd);
        for (int f = oldInt + 1; f <= endInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
        int beginInt = static_cast<int>(mFrameIndexBegin);
        for (int f = beginInt; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second.callback();
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }

    // 更新混合计时器
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        if (mReanimBlendCounter < 0) mReanimBlendCounter = 0;
    }

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->Update();
            }
        }
    }
}

void Animator::UpdateParallelDeferred(std::vector<DeferredEvent>& outBuf) {
    if (!mIsPlaying || !mReanim) return;

    float deltaTime = DeltaTime::GetDeltaTime();
    float oldFrame = mFrameIndexNow;

    float frameAdvance = deltaTime * mFPS * mSpeed * mExtraSpeedMultiplier;
    mFrameIndexNow += frameAdvance;

    bool reachedEnd = mFrameIndexNow >= mFrameIndexEnd;
    if (reachedEnd) {
        switch (mPlayingState) {
        case PlayState::PLAY_REPEAT:
            mFrameIndexNow = mFrameIndexBegin;
            break;
        case PlayState::PLAY_ONCE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        case PlayState::PLAY_ONCE_TO:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            if (!mTargetTrack.empty()) {
                PlayTrack(mTargetTrack, 0.0f, 0.5f);
                RestoreSpeed();
                mTargetTrack = "";
            }
            break;
        case PlayState::PLAY_NONE:
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        }
    }

    mFrameIndexNow = std::clamp(mFrameIndexNow, mFrameIndexBegin, mFrameIndexEnd);

    int oldInt = static_cast<int>(oldFrame);
    int newInt = static_cast<int>(mFrameIndexNow);

    if (newInt >= oldInt) {
        for (int f = oldInt + 1; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                outBuf.push_back({ it->second.callback });
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }
    else {
        int endInt = static_cast<int>(mFrameIndexEnd);
        for (int f = oldInt + 1; f <= endInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                outBuf.push_back({ it->second.callback });
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
        int beginInt = static_cast<int>(mFrameIndexBegin);
        for (int f = beginInt; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                outBuf.push_back({ it->second.callback });
                if (it->second.persistent) {
                    ++it;
                } else {
                    it = mFrameEvents.erase(it);
                }
            }
        }
    }

    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        if (mReanimBlendCounter < 0) mReanimBlendCounter = 0;
    }

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->UpdateParallelDeferred(outBuf);
            }
        }
    }
}

void Animator::Draw(Graphics* g, float baseX, float baseY, float Scale) {
    if (!mReanim || !g) return;

    // 保存当前变换栈，确保不叠加额外变换
    g->PushTransform(glm::mat4(1.0f));
    DrawInternal(g, baseX, baseY, Scale);
    g->PopTransform();
}

namespace {
    // Pack RGBA8 with r=lsb, a=msb — matches reanim_inst.vert.glsl unpack convention.
    inline uint32_t PackRGBA8(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        return  static_cast<uint32_t>(r)
             | (static_cast<uint32_t>(g) <<  8)
             | (static_cast<uint32_t>(b) << 16)
             | (static_cast<uint32_t>(a) << 24);
    }
}

void Animator::DrawInternalInstanced(Graphics* g, float baseX, float baseY, float Scale) const {
    static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    float blendRatio = 0.0f;
    if (mReanimBlendCounter > 0.0f)
        blendRatio = 1.0f - mReanimBlendCounter / mReanimBlendCounterMax;

    for (int i = 0; i < static_cast<int>(mReanim->GetTrackCount()); ++i) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        const TrackFrameTransform transform = GetInterpolatedTransform(i, blendRatio);

        bool visible = (i < static_cast<int>(mExtraInfos.size())) &&
                       mExtraInfos[i].mVisible && transform.f != -1;
        if (!visible) continue;

        const Texture* image = mExtraInfos[i].mImage ? mExtraInfos[i].mImage : transform.image;
        if (!image) continue;

        // CPU still computes the trig — GATE A measured this is ~6 ms CPU sum across
        // 165k tracks/frame; the GPU instancing win comes from removing per-call mat4
        // construction + 6-vertex inflation + write traffic (7× write bandwidth reduction),
        // not from moving trig itself.
        const float angleX = -transform.kx * DEG_TO_RAD;
        const float angleY = -transform.ky * DEG_TO_RAD;
        const float cosX = cosf(angleX);
        const float sinX = sinf(angleX);
        const float cosY = cosf(angleY);
        const float sinY = sinf(angleY);

        const float w = static_cast<float>(image->width);
        const float h = static_cast<float>(image->height);
        const float tx = transform.x + mExtraInfos[i].mOffsetX;
        const float ty = transform.y + mExtraInfos[i].mOffsetY;

        InstanceRecord rec;
        // Bake (w*Scale) into column 0, (h*Scale) into column 1 so the shader's `corner`
        // is unit. Layout matches Animator.cpp slow-path mat4 construction (lines ~379-384):
        //   mat[0] = (tA*w*Scale, tB*w*Scale, 0, 0)
        //   mat[1] = (tC*h*Scale, tD*h*Scale, 0, 0)
        //   mat[3] = (baseX + tx*Scale, baseY + ty*Scale, 0, 1)
        rec.tA = cosX * transform.sx * w * Scale;
        rec.tB = -sinX * transform.sx * w * Scale;
        rec.tC = sinY  * transform.sy * h * Scale;
        rec.tD = cosY  * transform.sy * h * Scale;
        rec.tx = baseX + tx * Scale;
        rec.ty = baseY + ty * Scale;

        // Atlas UV resolution: if image is part of an atlas page, use the page's bindless
        // slot id and the per-sprite atlas UV bbox; otherwise use image itself.
        const Texture* bindTex = image->atlasPage ? image->atlasPage : image;
        rec.u0 = image->aU0;
        rec.v0 = image->aV0;
        rec.u1 = image->aU1;
        rec.v1 = image->aV1;
        rec.texSlot = bindTex->id;

        // Normal draw: white tint × alpha
        const float baseAlpha = std::clamp(transform.a * mAlpha, 0.0f, 1.0f);
        const uint8_t alpha8 = static_cast<uint8_t>(baseAlpha * 255.0f);
        rec.colorRGBA8 = PackRGBA8(255, 255, 255, alpha8);
        g->AppendReanimInstance(rec, BlendMode::Alpha);

        // Glow (Add blend) — same geometry/UV, different color + BlendMode
        if (mEnableExtraAdditiveDraw) {
            InstanceRecord glow = rec;
            glow.colorRGBA8 = PackRGBA8(mExtraAdditiveColor.r,
                                        mExtraAdditiveColor.g,
                                        mExtraAdditiveColor.b,
                                        mExtraAdditiveColor.a);
            g->AppendReanimInstance(glow, BlendMode::Add);
        }

        // Overlay (Alpha blend, color alpha scaled by baseAlpha to match slow path)
        if (mEnableExtraOverlayDraw) {
            InstanceRecord ov = rec;
            const uint8_t ovAlpha = static_cast<uint8_t>(mExtraOverlayColor.a * baseAlpha);
            ov.colorRGBA8 = PackRGBA8(mExtraOverlayColor.r,
                                      mExtraOverlayColor.g,
                                      mExtraOverlayColor.b,
                                      ovAlpha);
            g->AppendReanimInstance(ov, BlendMode::Alpha);
        }
    }
}

void Animator::DrawInternal(Graphics* g, float baseX, float baseY, float Scale) const {
    if (!mReanim) return;

    // Task 5: fast path — if no track has attached child animators, route through
    // the GPU-instancing pipeline (AppendReanimInstance). The slow fallback (mat4 + DrawTextureMatrix)
    // remains for the ~1% case where animators carry children (e.g. PeaShooter anim_stem→head).
    // GATE A measured this fork's "fast path" rate at ~100% in the 11000-zombie stress scene.
    bool hasChildren = false;
    for (size_t i = 0; i < mExtraInfos.size(); ++i) {
        if (!mExtraInfos[i].mAttachedReanims.empty()) {
            hasChildren = true;
            break;
        }
    }
    if (!hasChildren && g->IsInstancePathEnabled()) {
        DrawInternalInstanced(g, baseX, baseY, Scale);
        return;
    }

    static constexpr float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    // 预计算混合比例，避免在轨道循环内每次做浮点除法
    float blendRatio = 0.0f;
    if (mReanimBlendCounter > 0.0f)
        blendRatio = 1.0f - mReanimBlendCounter / mReanimBlendCounterMax;

    for (int i = 0; i < static_cast<int>(mReanim->GetTrackCount()); ++i) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i, blendRatio);

        // 对本轨道 transform 只计算一次三角，自绘块和子动画块共用
        float angleX = -transform.kx * DEG_TO_RAD;
        float angleY = -transform.ky * DEG_TO_RAD;
        float cosX = cosf(angleX);
        float sinX = sinf(angleX);
        float cosY = cosf(angleY);
        float sinY = sinf(angleY);
        float tA = cosX * transform.sx;
        float tB = -sinX * transform.sx;
        float tC = sinY * transform.sy;
        float tD = cosY * transform.sy;

        bool shouldDrawSelf = (i < static_cast<int>(mExtraInfos.size()) &&
            mExtraInfos[i].mVisible &&
            transform.f != -1);
        const Texture* image = nullptr;

        if (shouldDrawSelf) {
            image = mExtraInfos[i].mImage ? mExtraInfos[i].mImage : transform.image;
            shouldDrawSelf = (image != nullptr);
        }

        if (shouldDrawSelf) {
            int imgWidth = image->width;
            int imgHeight = image->height;
            float w = static_cast<float>(imgWidth);
            float h = static_cast<float>(imgHeight);

            float tx = transform.x + mExtraInfos[i].mOffsetX;
            float ty = transform.y + mExtraInfos[i].mOffsetY;

            // 直接构造仿射变换矩阵（将单位矩形映射到目标四边形，省去中间顶点计算）
            glm::mat4 mat(
                tA * w * Scale,  tB * w * Scale,  0.0f, 0.0f,
                tC * h * Scale,  tD * h * Scale,  0.0f, 0.0f,
                0.0f,            0.0f,             1.0f, 0.0f,
                baseX + tx * Scale, baseY + ty * Scale, 0.0f, 1.0f
            );

            float combinedAlpha = transform.a * mAlpha;
            float baseAlpha = std::clamp(combinedAlpha, 0.0f, 1.0f);

            // 正常绘制
            glm::vec4 baseColor(255.0f, 255.0f, 255.0f, baseAlpha * 255.0f);
            g->DrawTextureMatrix(image, mat, 0.0f, 0.0f, baseColor, BlendMode::Alpha);

            // 发光效果（叠加混合）
            if (mEnableExtraAdditiveDraw) {
                glm::vec4 glowColor(mExtraAdditiveColor.r,
                    mExtraAdditiveColor.g,
                    mExtraAdditiveColor.b,
                    mExtraAdditiveColor.a);
                g->DrawTextureMatrix(image, mat, 0.0f, 0.0f, glowColor, BlendMode::Add);
            }

            // 覆盖层效果（Alpha 混合，颜色需乘以基础透明度）
            if (mEnableExtraOverlayDraw) {
                glm::vec4 overlayColor(mExtraOverlayColor.r,
                    mExtraOverlayColor.g,
                    mExtraOverlayColor.b,
                    mExtraOverlayColor.a * baseAlpha);
                g->DrawTextureMatrix(image, mat, 0.0f, 0.0f, overlayColor, BlendMode::Alpha);
            }
        }

        // 子动画
        if (i < static_cast<int>(mExtraInfos.size())) {
            for (const auto& weakChild : mExtraInfos[i].mAttachedReanims) {
                auto child = weakChild.lock();
                if (!child || !child->mReanim) continue;

                // 复用本轨道已计算的 tA/tB/tC/tD，无需重复三角运算
                float tx = transform.x + mExtraInfos[i].mOffsetX;
                float ty = transform.y + mExtraInfos[i].mOffsetY;

                float childX = child->mLocalPosX;
                float childY = child->mLocalPosY;

                float worldX = tx + tA * childX + tC * childY;
                float worldY = ty + tB * childX + tD * childY;

                worldX = baseX + worldX * Scale;
                worldY = baseY + worldY * Scale;

                child->DrawInternal(g, worldX, worldY, 1.0f);
            }
        }
    }
}

void Animator::SetSpeed(float speed) {
    this->mSpeed = speed;

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetSpeed(speed);
            }
        }
    }
}

void Animator::SetAlpha(float alpha) {
    this->mAlpha = alpha;

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetAlpha(alpha);
            }
        }
    }
}

void Animator::SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    this->mExtraAdditiveColor = { r, g, b, a };

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetGlowColor(r, g, b, a);
            }
        }
    }
}

void Animator::SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a) {
    this->mExtraOverlayColor = { r, g, b, a };

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetOverlayColor(r, g, b, a);
            }
        }
    }
}

void Animator::EnableGlowEffect(bool enable) {
    this->mEnableExtraAdditiveDraw = enable;

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->EnableGlowEffect(enable);
            }
        }
    }
}

void Animator::EnableOverlayEffect(bool enable) {
    this->mEnableExtraOverlayDraw = enable;

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->EnableOverlayEffect(enable);
            }
        }
    }
}

void Animator::SetTrackVisible(const std::string& trackName, bool visible) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mVisible = visible;
    }
}

void Animator::SetTrackImage(const std::string& trackName, const Texture* image) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mImage = image;
    }
}

void Animator::SetLocalPosition(float x, float y) {
    mLocalPosX = x;
    mLocalPosY = y;
}

void Animator::SetLocalPosition(const Vector& position) {
    this->SetLocalPosition(position.x, position.y);
}

void Animator::SetLocalScale(float sx, float sy) {
    mLocalScaleX = sx;
    mLocalScaleY = sy;
}

void Animator::SetLocalRotation(float rotation) {
    mLocalRotation = rotation;
}

bool Animator::AttachAnimator(const std::string& trackName, std::shared_ptr<Animator> child) {
    if (!mReanim || !child || child.get() == this) {
        return false;
    }

    auto extras = GetTrackExtrasByName(trackName);
    if (extras.empty()) {
        return false;
    }

    for (auto* extra : extras) {
        // 避免重复添加
        bool alreadyExists = false;
        for (const auto& weak : extra->mAttachedReanims) {
            if (auto existing = weak.lock()) {
                if (existing == child) {
                    alreadyExists = true;
                    break;
                }
            }
        }
        if (!alreadyExists) {
            extra->mAttachedReanims.push_back(child);
        }
    }
    return true;
}

void Animator::DetachAnimator(const std::string& trackName, std::shared_ptr<Animator> child) {
    auto extras = GetTrackExtrasByName(trackName);
    for (auto* extra : extras) {
        auto& vec = extra->mAttachedReanims;
        vec.erase(std::remove_if(vec.begin(), vec.end(),
            [&child](const std::weak_ptr<Animator>& weak) {
                auto sp = weak.lock();
                return sp == child || !sp; // 移除指定对象或已失效的
            }),
            vec.end());
    }
}

void Animator::DetachAllAnimators() {
    for (auto& extra : mExtraInfos) {
        extra.mAttachedReanims.clear();
    }
}

std::pair<int, int> Animator::GetTrackRange(const std::string& trackName) {
    if (!mReanim) {
        std::cout << "GetTrackRange: mReanim is null" << std::endl;
        return { -1, -1 };
    }

    TrackInfo* track = mReanim->GetTrack(trackName);
    if (!track || track->mFrames.empty()) {
        std::cout << "GetTrackRange: track '" << trackName << "' not found or empty" << std::endl;
        return { -1, -1 };
    }

    int totalFrames = static_cast<int>(track->mFrames.size());

    int start = -1;
    for (int i = 0; i < totalFrames; ++i) {
        if (track->mFrames[i].f == 0) {
            start = i;
            break;
        }
    }

    if (start == -1) {
        std::cout << "GetTrackRange: no f=0 frames, returning invalid." << std::endl;
        return { -1, -1 };
    }

    int end = start;
    for (int i = start + 1; i < totalFrames; ++i) {
        if (track->mFrames[i].f == 0) {
            end = i;
        }
        else if (track->mFrames[i].f == -1) {
            break;
        }
        else {
            std::cout << "GetTrackRange: unexpected f=" << track->mFrames[i].f << " at " << i << ", stopping." << std::endl;
            break;
        }
    }

    return { start, end };
}

void Animator::SetFrameRange(int frameBegin, int frameEnd) {
    mFrameIndexBegin = static_cast<float>(frameBegin);
    mFrameIndexEnd = static_cast<float>(frameEnd);
    mFrameIndexNow = static_cast<float>(frameBegin);
}

void Animator::SetFrameRangeByTrackName(const std::string& trackName) {
    auto range = GetTrackRange(trackName);
    SetFrameRange(range.first, range.second);
}

void Animator::SetFrameRangeToDefault() {
    if (mReanim) {
        mFrameIndexBegin = 0;
        mFrameIndexEnd = static_cast<float>(mReanim->GetTotalFrames() - 1);
    }
}

float Animator::GetTrackVelocity(const std::string& trackName) const {
    int index = GetFirstTrackIndexByName(trackName);
    if (index < 0) return 0.0f;
    return GetTrackVelocity(index);
}

float Animator::GetTrackVelocity(int trackIndex) const {
    if (!mReanim) return 0.0f;

    auto* track = mReanim->GetTrack(trackIndex);
    if (!track || track->mFrames.empty()) return 0.0f;

    int frameBefore = static_cast<int>(mFrameIndexNow);
    int maxIndex = static_cast<int>(track->mFrames.size()) - 1;
    frameBefore = std::clamp(frameBefore, 0, maxIndex);
    int frameAfter = std::min(frameBefore + 1, maxIndex);

    float xBefore = track->mFrames[frameBefore].x;
    float xAfter = track->mFrames[frameAfter].x;
    float dx = xAfter - xBefore;

    float velocity = dx * mSpeed * mExtraSpeedMultiplier;
    return std::abs(velocity);
}

void Animator::SetExtraSpeedMultiplier(float mul) {
    mExtraSpeedMultiplier = mul;

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetExtraSpeedMultiplier(mul);
            }
        }
    }
}

std::vector<TrackInfo*> Animator::GetTracksByName(const std::string& trackName) const {
    std::vector<TrackInfo*> result;
    if (!mReanim) return result;

    for (int i = 0; i < mReanim->GetTrackCount(); i++) {
        auto track = mReanim->GetTrack(i);
        if (track && track->mTrackName == trackName) {
            result.push_back(track);
        }
    }
    return result;
}

Vector Animator::GetTrackPosition(const std::string& trackName) const {
    auto tracks = GetTracksByName(trackName);
    for (auto track : tracks) {
        if (!track->mFrames.empty()) {
            int frameIndex = static_cast<int>(mFrameIndexNow);
            if (frameIndex < static_cast<int>(track->mFrames.size())) {
                return Vector(track->mFrames[frameIndex].x, track->mFrames[frameIndex].y);
            }
        }
    }
    return Vector::zero();
}

float Animator::GetTrackRotation(const std::string& trackName) const {
    auto tracks = GetTracksByName(trackName);
    for (auto track : tracks) {
        if (!track->mFrames.empty()) {
            int frameIndex = static_cast<int>(mFrameIndexNow);
            if (frameIndex < static_cast<int>(track->mFrames.size())) {
                return track->mFrames[frameIndex].kx;
            }
        }
    }
    return 0.0f;
}

bool Animator::GetTrackVisible(const std::string& trackName) const {
    int index = GetFirstTrackIndexByName(trackName);
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        return mExtraInfos[index].mVisible;
    }
    return false;
}

TrackFrameTransform Animator::GetInterpolatedTransform(int trackIndex, float blendRatio) const {
    TrackFrameTransform result;
    if (!mReanim) return result;

    auto track = mReanim->GetTrack(trackIndex);
    if (!track || track->mFrames.empty()) return result;

    int frameBefore = static_cast<int>(mFrameIndexNow);
    float fraction = mFrameIndexNow - frameBefore;
    int frameAfter = std::min(frameBefore + 1, static_cast<int>(track->mFrames.size() - 1));

    if (mReanimBlendCounter > 0) {
        // 过渡动画插值（blendRatio 由调用方预计算，避免此处重复做除法）
        GetDeltaTransform(track->mFrames[mFrameIndexBlendBuffer],
            track->mFrames[frameBefore],
            blendRatio,
            result, true);
    }
    else {
        // 正常帧间插值
        if (frameBefore >= 0 && frameAfter < static_cast<int>(track->mFrames.size())) {
            GetDeltaTransform(track->mFrames[frameBefore],
                track->mFrames[frameAfter],
                fraction, result);
        }
        else {
            result = track->mFrames[frameBefore];
        }
    }

    // 设置纹理指针：取前一帧的纹理
    if (frameBefore >= 0 && frameBefore < static_cast<int>(track->mFrames.size())) {
        result.image = track->mFrames[frameBefore].image;
    }

    return result;
}

std::vector<TrackExtraInfo*> Animator::GetTrackExtrasByName(const std::string& trackName) {
    std::vector<TrackExtraInfo*> result;
    for (int i = 0; i < static_cast<int>(mExtraInfos.size()); i++) {
        auto track = mReanim->GetTrack(i);
        if (track && track->mTrackName == trackName) {
            result.push_back(&mExtraInfos[i]);
        }
    }
    return result;
}

int Animator::GetFirstTrackIndexByName(const std::string& trackName) const {
    auto it = mTrackIndicesMap.find(trackName);
    return (it != mTrackIndicesMap.end()) ? it->second : -1;
}

// 颜色混合函数
int ColorComponentMultiply(int theColor1, int theColor2) {
    return std::clamp(theColor1 * theColor2 / 255, 0, 255);
}

SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2) {
    return SDL_Color{
        static_cast<Uint8>(ColorComponentMultiply(theColor1.r, theColor2.r)),
        static_cast<Uint8>(ColorComponentMultiply(theColor1.g, theColor2.g)),
        static_cast<Uint8>(ColorComponentMultiply(theColor1.b, theColor2.b)),
        static_cast<Uint8>(ColorComponentMultiply(theColor1.a, theColor2.a))
    };
}