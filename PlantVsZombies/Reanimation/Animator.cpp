#include "Animator.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include <algorithm>

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
        mDeltaRate = 1000.0f / mFPS;

        // 初始化轨道映射和额外信息
        mTrackIndicesMap.clear();
        mExtraInfos.clear();

        for (int i = 0; i < reanim->GetTrackCount(); i++) {
            auto track = reanim->GetTrack(i);
            if (track) {
                mTrackIndicesMap[track->mTrackName] = i;
                mExtraInfos.push_back(TrackExtraInfo());
            }
        }

        // 设置默认帧范围
        if (reanim->GetTrackCount() > 0) {
            mFrameIndexEnd = static_cast<float>(reanim->GetTotalFrames() - 1);
        }
    }
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

    // 平滑的帧前进计算
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
    }
    else {
        float frameAdvance = (deltaTime * mFPS * mSpeed);

        // 插值平滑
        mFrameIndexNow += frameAdvance;

        // 处理循环和结束
        if (mFrameIndexNow >= mFrameIndexEnd) {
            switch (mPlayingState) {
            case PlayState::PLAY_REPEAT:
                mFrameIndexNow = mFrameIndexBegin +
                    std::fmod(mFrameIndexNow - mFrameIndexBegin,
                        mFrameIndexEnd - mFrameIndexBegin);
                break;
            case PlayState::PLAY_ONCE_TO:
                SetFrameRangeByTrackName(mTargetTrack);
                mPlayingState = PlayState::PLAY_REPEAT;
                break;
            case PlayState::PLAY_ONCE:
                mFrameIndexNow = mFrameIndexEnd;
                mIsPlaying = false;
                // 触发完成事件
                break;
            case PlayState::PLAY_NONE:
                break;
            }
        }

        // 确保在范围内
        mFrameIndexNow = std::clamp(mFrameIndexNow,
            mFrameIndexBegin,
            mFrameIndexEnd);
    }
}

std::pair<int, int> Animator::GetTrackRange(const std::string& trackName) {
    if (!mReanim) return { 0, 0 };
    return mReanim->GetTrackFrameRange(trackName);
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

void Animator::SetFrameRangeByTrackNameOnce(const std::string& trackName, const std::string& oriTrackName) {
    SetFrameRangeByTrackName(trackName);
    mPlayingState = PlayState::PLAY_ONCE_TO;
    mTargetTrack = oriTrackName;
}

void Animator::SetTrackVisibleByTrackName(const std::string& trackName, bool visible) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mVisible = visible;
    }
}

void Animator::SetTrackVisible(int index, bool visible) {
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        mExtraInfos[index].mVisible = visible;
    }
}

void Animator::TrackAttachImageByTrackName(const std::string& trackName, SDL_Texture* target) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mAttachedImage = target;
    }
}

void Animator::TrackAttachImage(int index, SDL_Texture* target) {
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        mExtraInfos[index].mAttachedImage = target;
    }
}

void Animator::TrackAttachAnimator(const std::string& trackName, Animator* target) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mAttachedReanim = target;
    }
}

void Animator::TrackAttachAnimatorMatrix(const std::string& trackName, SexyTransform2D* target) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mAttachedReanimMatrix = target;
    }
}

void Animator::TrackAttachOffsetByTrackName(const std::string& trackName, float offsetX, float offsetY) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mOffsetX = offsetX;
        extra->mOffsetY = offsetY;
    }
}

void Animator::TrackAttachOffset(int index, float offsetX, float offsetY) {
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        mExtraInfos[index].mOffsetX = offsetX;
        mExtraInfos[index].mOffsetY = offsetY;
    }
}

void Animator::TrackAttachFlashSpot(int index, float spot) {
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        mExtraInfos[index].mFlashSpotSingle = spot;
    }
}

void Animator::Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale) {
    if (!mReanim || !renderer) return;

    // 使用整数位置以避免模糊
    int baseXi = static_cast<int>(baseX);
    int baseYi = static_cast<int>(baseY);

    // 保存当前渲染器状态
    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);

    // 设置渲染器为混合模式
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < mReanim->GetTrackCount(); i++) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        int frameIndex = static_cast<int>(mFrameIndexNow);
        if (frameIndex >= 0 && frameIndex < static_cast<int>(track->mFrames.size())) {
            if (track->mFrames[frameIndex].f == -1) {
                continue;
            }
        }

        if (i >= static_cast<int>(mExtraInfos.size()) || !mExtraInfos[i].mVisible) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i);

        SDL_Texture* image = mExtraInfos[i].mAttachedImage;
        if (!image && !transform.i.empty()) {
            if (mReanim->mResourceManager) {
                image = mReanim->mResourceManager->GetTexture(transform.i);
            }
        }
        if (!image) continue;

        // 获取纹理原始尺寸
        int imgWidth, imgHeight;
        SDL_QueryTexture(image, NULL, NULL, &imgWidth, &imgHeight);

        // 使用ceil而不是round，确保没有缝隙
        int scaledWidth = static_cast<int>(std::ceil(imgWidth * transform.sx * Scale));
        int scaledHeight = static_cast<int>(std::ceil(imgHeight * transform.sy * Scale));

        // 确保最小尺寸为1
        if (scaledWidth < 1) scaledWidth = 1;
        if (scaledHeight < 1) scaledHeight = 1;

        // 整数位置计算
        int posX = baseXi + static_cast<int>(std::floor((transform.x + mExtraInfos[i].mOffsetX) * Scale + 0.5f));
        int posY = baseYi + static_cast<int>(std::floor((transform.y + mExtraInfos[i].mOffsetY) * Scale + 0.5f));

        // 创建目标矩形
        SDL_Rect dstRect = { posX, posY, scaledWidth, scaledHeight };

        // 设置纹理透明度：帧透明度 × Animator的mAlpha
        float combinedAlpha = transform.a * mAlpha;
        Uint8 finalAlpha = static_cast<Uint8>(std::clamp(combinedAlpha * 255.0f, 0.0f, 255.0f));
        SDL_SetTextureAlphaMod(image, finalAlpha);

        // 设置颜色调制为白色（避免颜色混合）
        SDL_SetTextureColorMod(image, 255, 255, 255);

        // 旋转中心点
        SDL_Point center = { 0, 0 };

        // 使用 RenderCopyEx 渲染
        SDL_RenderCopyEx(renderer, image, NULL, &dstRect,
            transform.kx, &center, SDL_FLIP_NONE);

        // 恢复纹理设置
        SDL_SetTextureAlphaMod(image, 255);
    }

    // 恢复渲染器状态
    SDL_SetRenderDrawBlendMode(renderer, oldBlendMode);
}

void Animator::TrackAttachFlashSpotByTrackName(const std::string& trackName, float spot) {
    for (auto& extra : GetAllTracksExtraByName(trackName)) {
        extra->mFlashSpotSingle = spot;
    }
}

float Animator::GetTrackVelocity(const std::string& trackName) {
    if (!mReanim) return 0.0f;

    auto track = mReanim->GetTrack(trackName);
    if (!track || track->mFrames.size() < 2) return 0.0f;

    int currentFrame = static_cast<int>(mFrameIndexNow);
    if (currentFrame >= static_cast<int>(track->mFrames.size()) - 1) {
        currentFrame = static_cast<int>(track->mFrames.size()) - 2;
    }

    return track->mFrames[currentFrame + 1].x - track->mFrames[currentFrame].x;
}

bool Animator::GetTrackVisible(const std::string& trackName) {
    int index = GetFirstTrackExtraIndexByName(trackName);
    if (index >= 0 && index < static_cast<int>(mExtraInfos.size())) {
        return mExtraInfos[index].mVisible;
    }
    return false;
}

std::vector<TrackExtraInfo*> Animator::GetAllTracksExtraByName(const std::string& trackName) {
    std::vector<TrackExtraInfo*> result;
    for (int i = 0; i < static_cast<int>(mExtraInfos.size()); i++) {
        auto track = mReanim->GetTrack(i);
        if (track && track->mTrackName == trackName) {
            result.push_back(&mExtraInfos[i]);
        }
    }
    return result;
}

std::vector<TrackInfo*> Animator::GetAllTracksByName(const std::string& trackName) {
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

int Animator::GetFirstTrackExtraIndexByName(const std::string& trackName) {
    for (int i = 0; i < static_cast<int>(mExtraInfos.size()); i++) {
        auto track = mReanim->GetTrack(i);
        if (track && track->mTrackName == trackName) {
            return i;
        }
    }
    return -1;
}

Vector Animator::GetTrackPos(const std::string& trackname) {
    auto tracks = GetAllTracksByName(trackname);
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

float Animator::GetTrackRotate(const std::string& trackname) {
    auto tracks = GetAllTracksByName(trackname);
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