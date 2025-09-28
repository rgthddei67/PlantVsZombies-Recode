#include "Reanimator.h"
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
    mTracks.clear();
    mFPS = 12.0f;
    mReanimFileName.clear();
}

bool ReanimatorDefinition::LoadImages(SDL_Renderer* renderer) {
    if (!renderer) {
        TOD_TRACE("No renderer provided for loading images");
        return false;
    }

    TOD_TRACE("=== Loading images for: " + mReanimFileName + " ===");

    int loadedCount = 0;
    int failedCount = 0;
    int imageTransforms = 0;

    // 首先统计有多少transform有图像
    for (auto& track : mTracks) {
        for (auto& transform : track.mTransforms) {
            if (!transform.mImageName.empty()) {
                imageTransforms++;
            }
        }
    }

    TOD_TRACE("Found " + std::to_string(imageTransforms) + " transforms with images");

    // 只加载有图像的transform
    for (auto& track : mTracks) {
        for (auto& transform : track.mTransforms) {
            if (!transform.mImageName.empty() && !transform.mImage) {
                TOD_TRACE("Loading: " + transform.mImageName + " for track: " + track.mName);

                transform.mImage = ReanimParser::LoadReanimImage(transform.mImageName, renderer);
                if (transform.mImage) {
                    loadedCount++;
                }
                else {
                    failedCount++;
                    TOD_TRACE("FAILED to load: " + transform.mImageName);
                }
            }
        }
    }

    TOD_TRACE("Image loading completed - Success: " + std::to_string(loadedCount) +
        ", Failed: " + std::to_string(failedCount));

    // 只要成功加载了至少一个图像就返回true
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
    mReanimationType(ReanimationType::REANIM_NONE),
    mLastUpdateTime(0),
    mFirstUpdate(true) { 

    mOverlayMatrix.LoadIdentity();
}

Reanimation::~Reanimation() {
    // 不删除mDefinition，因为它可能被多个Reanimation共享
}

bool Reanimation::LoadReanimation(ReanimationType type, const std::string& reanimFile) {
    mReanimationType = type;
    mDefinition = new ReanimatorDefinition();

    if (!mDefinition->LoadFromFile(reanimFile)) {
        delete mDefinition;
        mDefinition = nullptr;
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


    if (!mDefinition->mTracks.empty()) {
        mFrameCount = static_cast<int>(mDefinition->mTracks[0].mTransforms.size());
    }

    TOD_TRACE("Loaded reanimation: " + reanimFile + " with " +
        std::to_string(mFrameCount) + " frames");
    return true;
}

void Reanimation::ReanimationInitialize(float x, float y, ReanimatorDefinition* definition) {
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

    // 使用高精度时间计算
    static Uint32 lastTime = SDL_GetTicks();
    Uint32 currentTime = SDL_GetTicks();

    // 处理时间回绕（32位整数溢出）
    if (currentTime < lastTime) {
        lastTime = currentTime;
        return;
    }

    float deltaTime;

    if (mFirstUpdate) {
        deltaTime = 0.0f;
        mFirstUpdate = false;
    }
    else {
        deltaTime = (currentTime - lastTime) / 1000.0f;
        if (deltaTime > 0.033f) { // 最大33ms，约30FPS的最低保证
            deltaTime = 0.033f;
        }
    }

    lastTime = currentTime;
    mLastFrameTime = mAnimTime;

    // 基于FPS的正确时间增量计算
    float timeDelta = deltaTime * (mAnimRate / mDefinition->mFPS);
    mAnimTime += timeDelta;

    // 根据循环类型处理时间
    switch (mLoopType) {
    case ReanimLoopType::REANIM_LOOP:
        while (mAnimTime >= 1.0f) {
            mLoopCount++;
            mAnimTime -= 1.0f;
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE:
        if (mAnimTime >= 1.0f) {
            mLoopCount = 1;
            mAnimTime = 1.0f;
            mDead = true;
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE_AND_HOLD:
        if (mAnimTime >= 1.0f) {
            mLoopCount = 1;
            mAnimTime = 1.0f;
        }
        break;

    case ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME:
        if (mAnimTime >= 1.0f) {
            mLoopCount++;
            mAnimTime -= 1.0f;
        }
        break;

    case ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME:
        if (mAnimTime >= 1.0f) {
            mLoopCount = 1;
            mAnimTime = 1.0f;
            mDead = true;
        }
        break;

    default:
        break;
    }

    // 调试输出
    static int updateCount = 0;
    updateCount++;
    if (updateCount % 60 == 0) { // 每秒输出一次，避免日志过多
        ReanimatorFrameTime frameTime;
        GetFrameTime(&frameTime);
        TOD_TRACE("Update - DeltaTime: " + std::to_string(deltaTime) +
            " AnimTime: " + std::to_string(mAnimTime) +
            " Frames: " + std::to_string(frameTime.mAnimFrameBeforeInt) + "->" + std::to_string(frameTime.mAnimFrameAfterInt));
    }
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

    // 计算目标矩形
    SDL_Rect destRect;
    destRect.w = static_cast<int>(texWidth * fabsf(transform.mScaleX)); // 使用绝对值避免负缩放
    destRect.h = static_cast<int>(texHeight * fabsf(transform.mScaleY));

    // 如果缩放为0，不绘制
    if (destRect.w <= 0 || destRect.h <= 0) {
        return;
    }

    // 计算位置
    float worldX = mOverlayMatrix.m02 + transform.mTransX;
    float worldY = mOverlayMatrix.m12 + transform.mTransY;

    destRect.x = static_cast<int>(worldX - destRect.w * 0.5f);
    destRect.y = static_cast<int>(worldY - destRect.h * 0.5f);

    // 设置透明度
    Uint8 alpha = static_cast<Uint8>(transform.mAlpha * 255);
    if (SDL_SetTextureAlphaMod(transform.mImage, alpha) != 0) {
        return; // 设置透明度失败
    }

    // 设置混合模式
    if (SDL_SetTextureBlendMode(transform.mImage, SDL_BLENDMODE_BLEND) != 0) {
        return; // 设置混合模式失败
    }

    // 计算旋转中心并绘制
    SDL_Point center = { destRect.w / 2, destRect.h / 2 };

    // 处理负缩放（翻转）
    SDL_RendererFlip flip = SDL_FLIP_NONE;
    if (transform.mScaleX < 0 || transform.mScaleY < 0) {
        // TODO 需要处理翻转，可以在这里添加逻辑
    }

    SDL_RenderCopyEx(mRenderer, transform.mImage, NULL, &destRect,
        transform.mSkewX, &center, flip);
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

void Reanimation::GetFrameTime(ReanimatorFrameTime* theFrameTime) {
    if (mFrameCount == 0) {
        theFrameTime->mFraction = 0.0f;
        theFrameTime->mAnimFrameBeforeInt = 0;
        theFrameTime->mAnimFrameAfterInt = 0;
        return;
    }

    // 正确的帧时间计算
    int aFrameCount;
    if (mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME ||
        mLoopType == ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME) {
        aFrameCount = mFrameCount;
    }
    else {
        aFrameCount = mFrameCount - 1;
    }

    float aAnimPosition = mFrameStart + mAnimTime * aFrameCount;
    float aAnimFrameBefore = floor(aAnimPosition);
    theFrameTime->mFraction = aAnimPosition - aAnimFrameBefore;
    theFrameTime->mAnimFrameBeforeInt = static_cast<int>(aAnimFrameBefore);

    if (theFrameTime->mAnimFrameBeforeInt >= mFrameStart + mFrameCount - 1) {
        theFrameTime->mAnimFrameBeforeInt = mFrameStart + mFrameCount - 1;
        theFrameTime->mAnimFrameAfterInt = theFrameTime->mAnimFrameBeforeInt;
    }
    else {
        theFrameTime->mAnimFrameAfterInt = theFrameTime->mAnimFrameBeforeInt + 1;
    }

    // 确保帧索引在有效范围内
    theFrameTime->mAnimFrameBeforeInt = std::max(mFrameStart, std::min(theFrameTime->mAnimFrameBeforeInt, mFrameStart + mFrameCount - 1));
    theFrameTime->mAnimFrameAfterInt = std::max(mFrameStart, std::min(theFrameTime->mAnimFrameAfterInt, mFrameStart + mFrameCount - 1));
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

    // 图像和文本使用前一帧的（不插值）
    theTransform->mImage = transformBefore.mImage;
    theTransform->mImageName = transformBefore.mImageName;
    theTransform->mFrame = transformBefore.mFrame;
}

// ====================================================================================================
// ReanimationHolder 实现
// ====================================================================================================

ReanimationHolder::ReanimationHolder(SDL_Renderer* renderer) :
    mRenderer(renderer) {
}

ReanimationHolder::~ReanimationHolder() {
    Clear();
}

Reanimation* ReanimationHolder::AllocReanimation(float x, float y, ReanimationType type, const std::string& reanimFile) {
    auto reanim = std::make_unique<Reanimation>(mRenderer);
    if (!reanim->LoadReanimation(type, reanimFile)) {
        TOD_TRACE("Failed to create reanimation: " + reanimFile);
        return nullptr;
    }

    reanim->SetPosition(x, y);
    Reanimation* ptr = reanim.get();
    mReanimations.push_back(std::move(reanim));
    return ptr;
}

Reanimation* ReanimationHolder::AllocReanimation(float x, float y, ReanimatorDefinition* definition) {
    auto reanim = std::make_unique<Reanimation>(mRenderer);
    reanim->ReanimationInitialize(x, y, definition);
    Reanimation* ptr = reanim.get();
    mReanimations.push_back(std::move(reanim));
    return ptr;
}

void ReanimationHolder::UpdateAll() {
    Uint32 currentTime = SDL_GetTicks();
    for (auto it = mReanimations.begin(); it != mReanimations.end(); ) 
    {
        (*it)->Update();
        if ((*it)->IsDead()) {
            it = mReanimations.erase(it);
        }
        else {
            ++it;
        }
    }
}

void ReanimationHolder::DrawAll() {
    // 按渲染顺序排序（如果需要）
    for (auto& reanim : mReanimations) {
        reanim->Draw();
    }
}

void ReanimationHolder::Clear() {
    mReanimations.clear();
}

Reanimation* ReanimationHolder::FindReanimationByType(ReanimationType type) {
    for (auto& reanim : mReanimations) {
        if (reanim->GetReanimationType() == type) {
            return reanim.get();
        }
    }
    return nullptr;
}

std::vector<Reanimation*> ReanimationHolder::FindReanimationsByType(ReanimationType type) {
    std::vector<Reanimation*> result;
    for (auto& reanim : mReanimations) {
        if (reanim->GetReanimationType() == type) {
            result.push_back(reanim.get());
        }
    }
    return result;
}

// ====================================================================================================
// 全局函数实现
// ====================================================================================================

void ReanimatorLoadDefinitions() {
    // 这里可以预加载常用的动画定义
    TOD_TRACE("Reanimator definitions loaded");
}

void ReanimatorFreeDefinitions() {
    // 清理全局动画定义资源
    TOD_TRACE("Reanimator definitions freed");
}

void ReanimationPreload(ReanimationType theReanimationType) {
    // 预加载特定类型的动画
    TOD_TRACE("Preloaded reanimation type: " + std::to_string(static_cast<int>(theReanimationType)));
}