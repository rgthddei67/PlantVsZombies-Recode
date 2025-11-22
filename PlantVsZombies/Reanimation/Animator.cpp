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
            mFrameIndexEnd = reanim->GetTotalFrames() - 1;
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

    // 过渡动画处理
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
    }
    else {
        // 正常更新
        float frameAdvance = (deltaTime * mFPS * mSpeed);
        mFrameIndexNow += frameAdvance;

        // 检查播放结束
        if (mFrameIndexNow >= mFrameIndexEnd) {
            switch (mPlayingState) {
            case PlayState::PLAY_REPEAT:
                mFrameIndexNow = mFrameIndexBegin;
                break;
            case PlayState::PLAY_ONCE_TO:
                SetFrameRangeByTrackName(mTargetTrack);
                mPlayingState = PlayState::PLAY_REPEAT;
                break;
            case PlayState::PLAY_ONCE:
                mFrameIndexNow = mFrameIndexEnd;
                mIsPlaying = false;
                break;
            case PlayState::PLAY_NONE:
                break;
            }
        }

        // 确保帧索引在范围内
        mFrameIndexNow = std::max(mFrameIndexBegin, std::min(mFrameIndexNow, mFrameIndexEnd));
    }
}

std::pair<int, int> Animator::GetTrackRange(const std::string& trackName) {
    if (!mReanim) return { 0, 0 };
    return mReanim->GetTrackFrameRange(trackName);
}

void Animator::SetFrameRange(int frameBegin, int frameEnd) {
    mFrameIndexBegin = frameBegin;
    mFrameIndexEnd = frameEnd;
    mFrameIndexNow = frameBegin;
}

void Animator::SetFrameRangeByTrackName(const std::string& trackName) {
    auto range = GetTrackRange(trackName);
    SetFrameRange(range.first, range.second);
}

void Animator::SetFrameRangeToDefault() {
    if (mReanim) {
        mFrameIndexBegin = 0;
        mFrameIndexEnd = mReanim->GetTotalFrames() - 1;
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

    for (int i = 0; i < mReanim->GetTrackCount(); i++) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;
        if (i >= static_cast<int>(mExtraInfos.size()) || !mExtraInfos[i].mVisible) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i);

        SDL_Texture* image = mExtraInfos[i].mAttachedImage;
        if (!image && !transform.i.empty()) {
            if (mReanim->mResourceManager) {
                image = mReanim->mResourceManager->GetTexture(transform.i);
            }
        }
        if (!image) continue;

        // 获取图像原始尺寸
        int imgWidth, imgHeight;
        SDL_QueryTexture(image, NULL, NULL, &imgWidth, &imgHeight);

        // 计算缩放后的尺寸 - 与参考代码一致
        int scaledWidth = static_cast<int>(imgWidth * transform.sx);
        int scaledHeight = static_cast<int>(imgHeight * transform.sy);

        // 使用左上角对齐，与参考代码一致
        float posX = baseX + transform.x + mExtraInfos[i].mOffsetX;
        float posY = baseY + transform.y + mExtraInfos[i].mOffsetY;

        // 目标矩形 - 左上角对齐
        SDL_Rect dstRect = {
            static_cast<int>(posX),
            static_cast<int>(posY),
            scaledWidth,
            scaledHeight
        };

        // 设置透明度
        SDL_SetTextureAlphaMod(image, static_cast<Uint8>(transform.a * 255));

        // 使用左上角作为旋转中心
        SDL_Point center = { 0, 0 };

        double rotation = transform.kx;

        // 旋转，指定左上角为旋转中心
        SDL_RenderCopyEx(renderer, image, NULL, &dstRect,
            rotation, &center, SDL_FLIP_NONE);

        // 恢复透明度
        SDL_SetTextureAlphaMod(image, 255);
    }
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