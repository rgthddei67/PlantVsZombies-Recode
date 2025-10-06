#include "Reanimator.h"
#include "ResourceManager.h"
#include "ReanimParser.h"
#include "TodCommon.h"
#include "TodDebug.h"
#include <SDL_image.h>
#include <algorithm>
#include <cmath>

// ====================================================================================================
// ReanimatorTransform 实现
// ====================================================================================================

ReanimatorTransform::ReanimatorTransform() :
    mTransX(0), mTransY(0),
    mSkewX(0), mSkewY(0),
    mScaleX(1.0f), mScaleY(1.0f),
    mFrame(-1.0f), mAlpha(1.0f),
    mImage(nullptr) {
}

// ====================================================================================================
// ReanimatorTrack 实现
// ====================================================================================================

ReanimatorTrack::ReanimatorTrack() :
    mName(""),
    mTransformCount(0) {
}

// ====================================================================================================
// ReanimatorDefinition 实现
// ====================================================================================================

ReanimatorDefinition::ReanimatorDefinition() :
    mFPS(12.0f),
    mReanimFileName("") {
}

ReanimatorDefinition::~ReanimatorDefinition() {
    Clear();
}

bool ReanimatorDefinition::LoadFromFile(const std::string& filename) {
    mReanimFileName = filename;
    return ReanimParser::LoadReanimFile(filename, this);
}

void ReanimatorDefinition::Clear() {
#ifdef _DEBUG
    TOD_TRACE("ReanimatorDefinition::Clear() - Starting cleanup");
#endif
    int textureCount = 0;
    int trackCount = mTracks.size();

    for (auto& track : mTracks) {
        for (auto& transform : track.mTransforms) {
            if (transform.mImage) {
                textureCount++;
                SDL_DestroyTexture(transform.mImage);
                transform.mImage = nullptr;
            }
            // 清理字符串
            transform.mImageName.clear();
        }
        // 清理轨道数据
        track.mName.clear();
        track.mTransforms.clear();
        track.mTransformCount = 0;
    }

    mTracks.clear();
    mFPS = 12.0f;
    mReanimFileName.clear();
#ifdef _DEBUG
    TOD_TRACE("ReanimatorDefinition::Clear() - Cleared " +
        std::to_string(textureCount) + " textures from " +
        std::to_string(trackCount) + " tracks");
#endif
}

bool ReanimatorDefinition::LoadImages(SDL_Renderer* renderer) {
    if (!renderer) {
        TOD_TRACE("No renderer provided for loading images");
        return false;
    }
#ifdef _DEBUG
    TOD_TRACE("=== Loading images for: " + mReanimFileName + " ===");
#endif
    int loadedCount = 0;
    int failedCount = 0;

    for (auto& track : mTracks) {
        for (auto& transform : track.mTransforms) {
            if (!transform.mImageName.empty() && !transform.mImage) {
#ifdef _DEBUG
                TOD_TRACE("Loading: " + transform.mImageName + " for track: " + track.mName);
#endif
                // 直接使用ReanimParser加载图像
                transform.mImage = ReanimParser::LoadReanimImage(transform.mImageName, renderer);
                if (transform.mImage) {
                    loadedCount++;
                }
                else {
                    failedCount++;
                    TOD_TRACE("FAILED to load: " + transform.mImageName);

                    // 尝试使用XFL位图加载（如果需要）
                    // 这里可以添加回退逻辑
                }
            }
        }
    }

    TOD_TRACE("Image loading completed - Success: " + std::to_string(loadedCount) +
        ", Failed: " + std::to_string(failedCount));

    return loadedCount > 0;
}

// ====================================================================================================
// Reanimation 实现
// ====================================================================================================

Reanimation::Reanimation(SDL_Renderer* renderer) :
    mDefinition(nullptr),
    mAnimTime(0),
    mAnimRate(12.0f),
    mLoopType(ReanimLoopType::REANIM_LOOP),
    mDead(false),
    mFrameStart(0),
    mFrameCount(0),
    mLoopCount(0),
    mIsAttachment(false),
    mRenderOrder(0),
    mLastFrameTime(-1.0f),
    mRenderer(renderer),
    mLastUpdateTime(0),
    mFirstUpdate(true),
    mScale(1.0f),
	mCurrentTime(1.0f),
	mTotalDuration(1.0f)
{ 
    mOverlayMatrix.LoadIdentity();
}

Reanimation::~Reanimation() {

}

bool Reanimation::LoadReanimation(const std::string& reanimFile) 
{
	mDefinition = std::make_shared<ReanimatorDefinition>();
    if (!mDefinition->LoadFromFile(reanimFile)) {
        mDefinition.reset();
		TOD_TRACE("Failed to load reanimation definition: " + reanimFile);
        return false;
    }

    if (mRenderer) {
        if (!mDefinition->LoadImages(mRenderer)) {
            TOD_TRACE("Failed to load images for reanimation: " + reanimFile);
            // 动画定义加载成功，图片加载失败
        }
    }
    else {
        TOD_TRACE("No renderer available for loading images: " + reanimFile);
    }

    if (mDefinition) {
        mAnimRate = mDefinition->mFPS;
        if (!mDefinition->mTracks.empty()) {
            mFrameCount = static_cast<int>(mDefinition->mTracks[0].mTransforms.size());
            mTotalDuration = mFrameCount / mDefinition->mFPS;
        }
    }

    if (!mDefinition->mTracks.empty()) {
        mFrameCount = static_cast<int>(mDefinition->mTracks[0].mTransforms.size());
    }

    TOD_TRACE("Loaded reanimation: " + reanimFile + " with " +
        std::to_string(mFrameCount) + " frames");
    return true;
}

void Reanimation::ReanimationInitialize(float x, float y, std::shared_ptr<ReanimatorDefinition> definition) {
    mDefinition = definition;
    mAnimRate = definition->mFPS;
    SetPosition(x, y);

    // 如果还没有加载图片，现在加载
    if (mRenderer) {
        bool needsImageLoading = false;
        for (auto& track : mDefinition->mTracks) {
            for (auto& transform : track.mTransforms) {
                if (!transform.mImageName.empty() && !transform.mImage) {
                    needsImageLoading = true;
                    break;
                }
            }
            if (needsImageLoading) break;
        }

        if (needsImageLoading) {
            mDefinition->LoadImages(mRenderer);
        }
    }

    if (!definition->mTracks.empty()) {
        mFrameCount = static_cast<int>(definition->mTracks[0].mTransforms.size());
    }
}

void Reanimation::Update() {
    if (mDead || !mDefinition || mDefinition->mTracks.empty() || mFrameCount == 0) {
        return;
    }

    static Uint32 lastTime = SDL_GetTicks();
    Uint32 currentTime = SDL_GetTicks();

    if (currentTime < lastTime) {
        lastTime = currentTime;
        return;
    }

    float deltaTime;
    if (mFirstUpdate) 
    {
        deltaTime = 0.0f;
        mFirstUpdate = false;

        // 初始化总时长
        mTotalDuration = mFrameCount / mDefinition->mFPS;
        mCurrentTime = 0.0f;
    }
    else {
        deltaTime = (currentTime - lastTime) / 1000.0f;
        if (deltaTime > 0.033f) {
            deltaTime = 0.033f;
        }
    }
    lastTime = currentTime;

    mLastFrameTime = mAnimTime;

    // 使用实际时间而不是归一化时间
    mCurrentTime += deltaTime * (mAnimRate / mDefinition->mFPS);

    // 根据循环类型处理时间
    switch (mLoopType) {
    case ReanimLoopType::REANIM_LOOP:
        if (mCurrentTime >= mTotalDuration) {
            mLoopCount++;
            mCurrentTime = fmod(mCurrentTime, mTotalDuration);
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE:
        if (mCurrentTime >= mTotalDuration) {
            mLoopCount = 1;
            mCurrentTime = mTotalDuration;
            mDead = true;
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD:
        if (mCurrentTime >= mTotalDuration) {
            mLoopCount = 1;
            mCurrentTime = mTotalDuration;
        }
        break;

    case ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME:
        // 循环播放（包含最后一帧）
        if (mCurrentTime >= mTotalDuration) {
            mLoopCount++;
            mCurrentTime = mTotalDuration;
            //TOD_TRACE("Full frame animation looped, reset to start");
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME:
        // 播放一次（包含最后一帧）并标记为dead
        if (mCurrentTime >= mTotalDuration) 
        {
            mLoopCount = 1;
            mCurrentTime = mTotalDuration;
            mDead = true;
        }
        break;

    default:
        break;
    }
    mAnimTime = mCurrentTime / mTotalDuration;
}

void Reanimation::Draw() {
    if (mDead || !mDefinition || !mRenderer || mDefinition->mTracks.empty()) {
        return;
    }

    for (size_t trackIndex = 0; trackIndex < mDefinition->mTracks.size(); trackIndex++) {
        DrawTrack(static_cast<int>(trackIndex), 0);
    }
}

void Reanimation::DrawTrack(int trackIndex, int frameIndex) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(mDefinition->mTracks.size())) {
        return;
    }

    // 使用插值变换
    ReanimatorFrameTime frameTime;
    GetFrameTime(&frameTime);

    ReanimatorTransform transform;
    GetTransformAtTime(trackIndex, &transform, &frameTime);

    ReanimatorTrack& track = mDefinition->mTracks[trackIndex];

    // 检查是否有有效的图像
    if (!transform.mImage || transform.mFrame < 0 || transform.mAlpha <= 0.01f) {
        return;
    }

    // 获取纹理尺寸
    int texWidth, texHeight;
    if (SDL_QueryTexture(transform.mImage, NULL, NULL, &texWidth, &texHeight) != 0) {
        return; // 纹理无效
    }

    if (texWidth <= 0 || texHeight <= 0) {
        return;
    }

    // 计算目标矩形 - 应用全局缩放
    SDL_Rect destRect;
    destRect.w = static_cast<int>(texWidth * fabsf(transform.mScaleX) * mScale);
    destRect.h = static_cast<int>(texHeight * fabsf(transform.mScaleY) * mScale);

    // 如果缩放为0，不绘制
    if (destRect.w <= 0 || destRect.h <= 0) {
        return;
    }

    // 计算位置 - 应用全局缩放
    float layerOffset = trackIndex * 2.0f * mScale; // 每层稍微偏移一点，便于调试
    float worldX = mOverlayMatrix.m02 + transform.mTransX * mScale + layerOffset;
    float worldY = mOverlayMatrix.m12 + transform.mTransY * mScale;

    destRect.x = static_cast<int>(worldX - destRect.w * 0.5f);
    destRect.y = static_cast<int>(worldY - destRect.h * 0.5f);

    // 设置透明度
    Uint8 alpha = static_cast<Uint8>(transform.mAlpha * 255);
    if (SDL_SetTextureAlphaMod(transform.mImage, alpha) != 0) {
        return;
    }

    // 设置混合模式
    if (SDL_SetTextureBlendMode(transform.mImage, SDL_BLENDMODE_BLEND) != 0) {
        return;
    }

    // 计算旋转中心并绘制
    SDL_Point center = { destRect.w / 2, destRect.h / 2 };

    SDL_RenderCopyEx(mRenderer, transform.mImage, NULL, &destRect,
        transform.mSkewX, &center, SDL_FLIP_NONE);
}

void Reanimation::SetPosition(float x, float y) {
    mOverlayMatrix.m02 = x;
    mOverlayMatrix.m12 = y;
}

void Reanimation::SetRate(float rate) {
    mAnimRate = rate;
}

void Reanimation::SetLoopType(ReanimLoopType loopType) {
    mLoopType = loopType;
}

int Reanimation::FindTrackIndex(const char* trackName) {
    if (!mDefinition || !trackName) {
        return -1;
    }

    for (size_t i = 0; i < mDefinition->mTracks.size(); i++) {
        if (mDefinition->mTracks[i].mName == trackName) {
            return static_cast<int>(i);
        }
    }

    return -1;
}

void Reanimation::SetFramesForLayer(const char* trackName) {
    int trackIndex = FindTrackIndex(trackName);
    if (trackIndex >= 0 && trackIndex < static_cast<int>(mDefinition->mTracks.size())) {
        mFrameStart = 0;
        mFrameCount = static_cast<int>(mDefinition->mTracks[trackIndex].mTransforms.size());
        mAnimTime = 0.0f;
    }
}

void Reanimation::GetFrameTime(ReanimatorFrameTime* theFrameTime)
{
    if (mFrameCount == 0) {
        theFrameTime->mFraction = 0.0f;
        theFrameTime->mAnimFrameBeforeInt = 0;
        theFrameTime->mAnimFrameAfterInt = 0;
        return;
    }

    float framePosition = (mCurrentTime / mTotalDuration) * (mFrameCount - 1);
    int frameBefore = static_cast<int>(framePosition);
    theFrameTime->mFraction = framePosition - frameBefore;
    theFrameTime->mAnimFrameBeforeInt = frameBefore;
    theFrameTime->mAnimFrameAfterInt = std::min(frameBefore + 1, mFrameCount - 1);
}

void Reanimation::GetTransformAtTime(int theTrackIndex, ReanimatorTransform* theTransform, ReanimatorFrameTime* theFrameTime) {
    if (theTrackIndex < 0 || theTrackIndex >= static_cast<int>(mDefinition->mTracks.size())) {
        return;
    }

    ReanimatorTrack& track = mDefinition->mTracks[theTrackIndex];

    // 确保有变换数据
    if (track.mTransforms.empty()) {
        return;
    }

    // 确保帧索引在有效范围内
    int frameBefore = std::max(0, std::min(theFrameTime->mAnimFrameBeforeInt, static_cast<int>(track.mTransforms.size()) - 1));
    int frameAfter = std::max(0, std::min(theFrameTime->mAnimFrameAfterInt, static_cast<int>(track.mTransforms.size()) - 1));

    ReanimatorTransform& transformBefore = track.mTransforms[frameBefore];
    ReanimatorTransform& transformAfter = track.mTransforms[frameAfter];

    // 如果前后帧相同，直接复制
    if (frameBefore == frameAfter || theFrameTime->mFraction < 0.001f) {
        *theTransform = transformBefore;
        return;
    }

    // 帧间插值计算
    theTransform->mTransX = FloatLerp(transformBefore.mTransX, transformAfter.mTransX, theFrameTime->mFraction);
    theTransform->mTransY = FloatLerp(transformBefore.mTransY, transformAfter.mTransY, theFrameTime->mFraction);
    theTransform->mSkewX = FloatLerp(transformBefore.mSkewX, transformAfter.mSkewX, theFrameTime->mFraction);
    theTransform->mSkewY = FloatLerp(transformBefore.mSkewY, transformAfter.mSkewY, theFrameTime->mFraction);
    theTransform->mScaleX = FloatLerp(transformBefore.mScaleX, transformAfter.mScaleX, theFrameTime->mFraction);
    theTransform->mScaleY = FloatLerp(transformBefore.mScaleY, transformAfter.mScaleY, theFrameTime->mFraction);
    theTransform->mAlpha = FloatLerp(transformBefore.mAlpha, transformAfter.mAlpha, theFrameTime->mFraction);

    // 优先使用有图像的帧
    if (transformBefore.mImage != nullptr) {
        theTransform->mImage = transformBefore.mImage;
        theTransform->mImageName = transformBefore.mImageName;
        theTransform->mFrame = transformBefore.mFrame;
    }
    else if (transformAfter.mImage != nullptr) {
        theTransform->mImage = transformAfter.mImage;
        theTransform->mImageName = transformAfter.mImageName;
        theTransform->mFrame = transformAfter.mFrame;
    }
}
