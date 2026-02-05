#include "Animator.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

Animator::Animator() {
    mReanim = nullptr;
}

Animator::Animator(std::shared_ptr<Reanimation> reanim) {
    Init(reanim);
}

void Animator::Init(std::shared_ptr<Reanimation> reanim) {
    mReanim = reanim;
    if (reanim) {
        mFPS = reanim->mFPS;

        // 初始化轨道映射和额外信息
        mTrackIndicesMap.clear();
        mExtraInfos.clear();
        mAnimationClips.clear();

        for (int i = 0; i < reanim->GetTrackCount(); i++) {
            auto track = reanim->GetTrack(i);
            if (track) {
                mTrackIndicesMap[track->mTrackName] = i;
                mExtraInfos.push_back(TrackExtraInfo());

                // 收集所有动画片段
                for (const auto& clip : track->mClips) {
                    if (clip.IsValid()) {
                        std::string animationKey = clip.trackName;
                        if (!clip.clipName.empty() && clip.clipName != "default") {
                            animationKey += "_" + clip.clipName;
                        }
                        mAnimationClips[animationKey] = clip;
                    }
                }
            }
        }

        // 默认设置为第一个有效的动画
        if (!mAnimationClips.empty()) {
            auto it = mAnimationClips.begin();
            SetAnimation(it->first);
        }

        mPlayingState = PlayState::PLAY_REPEAT;
        mIsPlaying = false;
        mTargetTrack = "";
    }
}

bool Animator::SetAnimation(const std::string& animationName) {
    auto it = mAnimationClips.find(animationName);
    if (it != mAnimationClips.end()) {
        mCurrentClip = it->second;
        mCurrentAnimationName = animationName;
        SetFrameRange(mCurrentClip.startFrame, mCurrentClip.endFrame);
        return true;
    }

    // 如果没有找到完全匹配的，尝试按轨道名查找
    for (const auto& pair : mAnimationClips) {
        if (pair.second.trackName == animationName) {
            mCurrentClip = pair.second;
            mCurrentAnimationName = pair.first;
            SetFrameRange(mCurrentClip.startFrame, mCurrentClip.endFrame);
            return true;
        }
    }

    std::cerr << "动画未找到: " << animationName << std::endl;
    return false;
}

bool Animator::SetAnimation(const std::string& trackName, const std::string& clipName) {
    std::string key = clipName.empty() ? trackName : trackName + "_" + clipName;
    return SetAnimation(key);
}

bool Animator::PlayTrack(const std::string& trackName, int blendTime) {
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
        mReanimBlendCounterMax = static_cast<float>(blendTime);
        mReanimBlendCounter = static_cast<float>(blendTime);
    }
    else {
        mReanimBlendCounter = -1.0f;
    }

    mCurrentAnimationName = trackName;
    mIsPlaying = true;
    mPlayingState = PlayState::PLAY_REPEAT;

#ifdef _DEBUG
    std::cout << "播放动画轨道: " << trackName
        << " (帧: " << range.first << "-" << range.second
        << ", 过渡: " << blendTime << "ms)" << std::endl;
#endif

    return true;
}

bool Animator::PlayTrackOnce(const std::string& trackName, const std::string& returnTrack, int blendTime) {
    if (!PlayTrack(trackName, blendTime)) {
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

    // 如果有混合过渡，更新混合计时器
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        return;
    }

    float frameAdvance = deltaTime * mFPS * mSpeed;
    mFrameIndexNow += frameAdvance;

    // 检查是否到达当前动画的结束
    bool reachedEnd = mFrameIndexNow >= mFrameIndexEnd;

    if (reachedEnd) {
        switch (mPlayingState) {
        case PlayState::PLAY_REPEAT:
            // 循环播放：回到起始帧
            mFrameIndexNow = mFrameIndexBegin;
            break;

        case PlayState::PLAY_ONCE:
            // 播放一次：停在最后一帧
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;

            // 自动切换回默认动画
            if (mAutoSwitchAnimation && !mTargetTrack.empty()) {
                PlayTrack(mTargetTrack, 200);
                mTargetTrack = "";
            }
            break;

        case PlayState::PLAY_ONCE_TO:
            // 播放到指定动画
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;

            // 切换到目标动画
            if (!mTargetTrack.empty()) {
                PlayTrack(mTargetTrack, 200);
                mTargetTrack = "";
            }
            break;

        case PlayState::PLAY_NONE:
            // 不播放：停在当前帧
            mFrameIndexNow = mFrameIndexEnd;
            mIsPlaying = false;
            break;
        }
    }

    // 确保帧在当前片段范围内
    mFrameIndexNow = std::clamp(mFrameIndexNow, mFrameIndexBegin, mFrameIndexEnd);
}

void Animator::SetTrackVisible(const std::string& trackName, bool visible) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mVisible = visible;
    }
}

void Animator::SetTrackImage(const std::string& trackName, SDL_Texture* image) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mAttachedImage = image;
    }
}

void Animator::SetTrackOffset(const std::string& trackName, float offsetX, float offsetY) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mOffsetX = offsetX;
        extra->mOffsetY = offsetY;
    }
}

void Animator::AttachAnimator(const std::string& trackName, Animator* animator) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mAttachedReanim = animator;
    }
}

std::pair<int, int> Animator::GetTrackRange(const std::string& trackName) {
    if (!mReanim) return { -1, -1 };

    TrackInfo* track = mReanim->GetTrack(trackName);
    if (!track || track->mFrames.empty()) {
        return { -1, -1 };
    }

    // 找到第一个f=0的帧作为动画开始
    int frameStart = -1;
    for (size_t i = 0; i < track->mFrames.size(); i++) {
        if (track->mFrames[i].f == 0) {
            frameStart = static_cast<int>(i);
            break;
        }
    }

    if (frameStart == -1) {
        return { -1, -1 };  // 没有显示帧
    }

    // 找到连续的显示帧（f=0）直到遇到f=-1
    int frameEnd = frameStart;
    for (size_t i = frameStart + 1; i < track->mFrames.size(); i++) {
        if (track->mFrames[i].f == -1) {  // 遇到空白帧/分隔帧
            frameEnd = static_cast<int>(i) - 1;
            break;
        }
        if (track->mFrames[i].f == 0) {  // 仍然是显示帧
            frameEnd = static_cast<int>(i);
        }
        // 如果f既不是0也不是-1，也认为片段结束（安全处理）
        if (track->mFrames[i].f != 0) {
            frameEnd = static_cast<int>(i) - 1;
            break;
        }
    }

    // 确保不会超过轨道范围
    if (frameEnd >= static_cast<int>(track->mFrames.size())) {
        frameEnd = static_cast<int>(track->mFrames.size()) - 1;
    }

    return { frameStart, frameEnd };
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

std::vector<std::string> Animator::GetAvailableAnimations() const {
    std::vector<std::string> animations;
    for (const auto& pair : mAnimationClips) {
        animations.push_back(pair.first);
    }
    return animations;
}

void Animator::Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale) {
    if (!mReanim || !renderer) return;

    // 保存当前渲染器状态
    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < mReanim->GetTrackCount(); i++) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        int frameIndex = static_cast<int>(mFrameIndexNow);
        if (frameIndex >= 0 && frameIndex < static_cast<int>(track->mFrames.size())) {
            // f=-1表示隐藏/空白帧
            if (track->mFrames[frameIndex].f == -1) {
                continue;
            }
        }

        if (i >= static_cast<int>(mExtraInfos.size()) || !mExtraInfos[i].mVisible) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i);
        if (transform.f == -1) continue;  // 插值后仍然可能是空白帧

        SDL_Texture* image = mExtraInfos[i].mAttachedImage;
        if (!image && !transform.image.empty()) {
            if (mReanim->mResourceManager) {
                image = mReanim->mResourceManager->GetTexture(transform.image);
            }
        }
        if (!image) continue;

        // 获取纹理原始尺寸
        int imgWidth, imgHeight;
        SDL_QueryTexture(image, NULL, NULL, &imgWidth, &imgHeight);

        // 计算位置和缩放
        float exactPosX = baseX + (transform.x + mExtraInfos[i].mOffsetX) * Scale;
        float exactPosY = baseY + (transform.y + mExtraInfos[i].mOffsetY) * Scale;

        float exactScaleX = transform.sx * Scale;
        float exactScaleY = transform.sy * Scale;

        // 计算浮点数尺寸
        float scaledWidth = imgWidth * exactScaleX;
        float scaledHeight = imgHeight * exactScaleY;

        // 确保最小尺寸
        if (scaledWidth < 1.0f) scaledWidth = 1.0f;
        if (scaledHeight < 1.0f) scaledHeight = 1.0f;

        // 创建目标矩形
        SDL_FRect dstRect = { exactPosX, exactPosY, scaledWidth, scaledHeight };

        // 计算透明度
        float combinedAlpha = transform.a * mAlpha;
        Uint8 baseAlpha = static_cast<Uint8>(std::clamp(combinedAlpha * 255.0f, 0.0f, 255.0f));

        // 旋转中心点为(0,0)（左上角）
        SDL_FPoint center = { 0.0f, 0.0f };

        // 第一步：正常绘制
        SDL_SetTextureAlphaMod(image, baseAlpha);
        SDL_SetTextureColorMod(image, 255, 255, 255);
        SDL_RenderCopyExF(renderer, image, NULL, &dstRect,
            transform.kx, &center, SDL_FLIP_NONE);

        // 第二步：发光效果
        if (mEnableExtraAdditiveDraw) {
            SDL_BlendMode textureBlendMode;
            SDL_GetTextureBlendMode(image, &textureBlendMode);

            SDL_SetTextureBlendMode(image, SDL_BLENDMODE_ADD);
            SDL_SetTextureColorMod(image, mExtraAdditiveColor.r,
                mExtraAdditiveColor.g, mExtraAdditiveColor.b);
            SDL_SetTextureAlphaMod(image, mExtraAdditiveColor.a);

            SDL_RenderCopyExF(renderer, image, NULL, &dstRect,
                transform.kx, &center, SDL_FLIP_NONE);

            SDL_SetTextureBlendMode(image, textureBlendMode);
            SDL_SetTextureColorMod(image, 255, 255, 255);
            SDL_SetTextureAlphaMod(image, 255);
        }

        // 第三步：覆盖层
        if (mEnableExtraOverlayDraw) {
            SDL_Color overlayColor = mExtraOverlayColor;
            overlayColor.a = ColorComponentMultiply(mExtraOverlayColor.a, baseAlpha);

            SDL_SetTextureColorMod(image, overlayColor.r, overlayColor.g, overlayColor.b);
            SDL_SetTextureAlphaMod(image, overlayColor.a);

            SDL_RenderCopyExF(renderer, image, NULL, &dstRect,
                transform.kx, &center, SDL_FLIP_NONE);

            SDL_SetTextureColorMod(image, 255, 255, 255);
            SDL_SetTextureAlphaMod(image, 255);
        }
    }

    // 恢复渲染器状态
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
}

// 获取轨道信息
std::vector<TrackInfo*> Animator::GetTracksByName(const std::string& trackName) {
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

Vector Animator::GetTrackPosition(const std::string& trackName) {
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

float Animator::GetTrackRotation(const std::string& trackName) {
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

bool Animator::GetTrackVisible(const std::string& trackName) {
    int index = GetFirstTrackIndexByName(trackName);
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        return mExtraInfos[index].mVisible;
    }
    return false;
}

TrackFrameTransform Animator::GetInterpolatedTransform(int trackIndex) const {
    TrackFrameTransform result;
    if (!mReanim) return result;

    auto track = mReanim->GetTrack(trackIndex);
    if (!track || track->mFrames.empty()) return result;

    int frameBefore = static_cast<int>(mFrameIndexNow);
    float fraction = mFrameIndexNow - frameBefore;

    int frameAfter = std::min(frameBefore + 1, static_cast<int>(track->mFrames.size() - 1));

    if (mReanimBlendCounter > 0) {
        // 过渡动画插值
        GetDeltaTransform(track->mFrames[mFrameIndexBlendBuffer],
            track->mFrames[frameBefore],
            1.0f - mReanimBlendCounter / mReanimBlendCounterMax,
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

int Animator::GetFirstTrackIndexByName(const std::string& trackName) {
    for (int i = 0; i < static_cast<int>(mExtraInfos.size()); i++) {
        auto track = mReanim->GetTrack(i);
        if (track && track->mTrackName == trackName) {
            return i;
        }
    }
    return -1;
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