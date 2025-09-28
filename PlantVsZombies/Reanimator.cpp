#include "Reanimator.h"
#include "ReanimParser.h"
#include "TodCommon.h"
#include "TodDebug.h"
#include <SDL_image.h>
#include <algorithm>
#include <cmath>

// ====================================================================================================
// ReanimatorTransform ʵ��
// ====================================================================================================

ReanimatorTransform::ReanimatorTransform() :
    mTransX(0), mTransY(0),
    mSkewX(0), mSkewY(0),
    mScaleX(1.0f), mScaleY(1.0f),
    mFrame(-1.0f), mAlpha(1.0f),
    mImage(nullptr) {
}

// ====================================================================================================
// ReanimatorTrack ʵ��
// ====================================================================================================

ReanimatorTrack::ReanimatorTrack() :
    mName(""),
    mTransformCount(0) {
}

// ====================================================================================================
// ReanimatorDefinition ʵ��
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

    // ����ͳ���ж���transform��ͼ��
    for (auto& track : mTracks) {
        for (auto& transform : track.mTransforms) {
            if (!transform.mImageName.empty()) {
                imageTransforms++;
            }
        }
    }

    TOD_TRACE("Found " + std::to_string(imageTransforms) + " transforms with images");

    // ֻ������ͼ���transform
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

    // ֻҪ�ɹ�����������һ��ͼ��ͷ���true
    return loadedCount > 0;
}

// ====================================================================================================
// Reanimation ʵ��
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
    mReanimationType(ReanimationType::REANIM_NONE) {

    mOverlayMatrix.LoadIdentity();
}

Reanimation::~Reanimation() {
    // ��ɾ��mDefinition����Ϊ�����ܱ����Reanimation����
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
            // ����������سɹ���ͼƬ����ʧ��
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

    // �����û�м���ͼƬ�����ڼ���
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

    mLastFrameTime = mAnimTime;
    float deltaTime = 0.016f; // ����60FPS

    // ����ʱ������
    float timeDelta = deltaTime * mAnimRate / mFrameCount;
    mAnimTime += timeDelta;

    // ����ѭ�����ʹ���ʱ��
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
        // �������һ֡��������ʾ
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
}

void Reanimation::Draw() {
    if (mDead || !mDefinition || !mRenderer || mDefinition->mTracks.empty()) {
        return;
    }

    int frameIndex = CalculateFrameIndex();
    if (frameIndex < 0 || frameIndex >= mFrameCount) {
        return;
    }

    // �������й��
    for (size_t trackIndex = 0; trackIndex < mDefinition->mTracks.size(); trackIndex++) {
        DrawTrack(static_cast<int>(trackIndex), frameIndex);
    }
}

void Reanimation::DrawTrack(int trackIndex, int frameIndex) {
    if (trackIndex < 0 || trackIndex >= static_cast<int>(mDefinition->mTracks.size())) {
        return;
    }

    ReanimatorTrack& track = mDefinition->mTracks[trackIndex];
    if (frameIndex < 0 || frameIndex >= static_cast<int>(track.mTransforms.size())) {
        return;
    }

    ReanimatorTransform& transform = track.mTransforms[frameIndex];

    // ����Ƿ�����Ч��ͼ��
    if (!transform.mImage || transform.mFrame < 0) {
        return;
    }

    // ��ȡ����ߴ�
    int texWidth, texHeight;
    SDL_QueryTexture(transform.mImage, NULL, NULL, &texWidth, &texHeight);

    if (texWidth <= 0 || texHeight <= 0) {
        return;
    }

    // ����Ŀ�����
    SDL_Rect destRect;
    destRect.w = static_cast<int>(texWidth * transform.mScaleX);
    destRect.h = static_cast<int>(texHeight * transform.mScaleY);
    destRect.x = static_cast<int>(mOverlayMatrix.m02 + transform.mTransX - destRect.w * 0.5f);
    destRect.y = static_cast<int>(mOverlayMatrix.m12 + transform.mTransY - destRect.h * 0.5f);

    // ����͸����
    Uint8 alpha = static_cast<Uint8>(transform.mAlpha * 255);
    SDL_SetTextureAlphaMod(transform.mImage, alpha);

    // ���û��ģʽ
    SDL_SetTextureBlendMode(transform.mImage, SDL_BLENDMODE_BLEND);

    // ������ת����
    SDL_Point center = { destRect.w / 2, destRect.h / 2 };

    // ��������֧����ת��
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

int Reanimation::CalculateFrameIndex() {
    if (mFrameCount == 0) return 0;

    int frameIndex;
    if (mLoopType == ReanimLoopType::REANIM_PLAY_ONCE_FULL_LAST_FRAME ||
        mLoopType == ReanimLoopType::REANIM_LOOP_FULL_LAST_FRAME) {
        frameIndex = static_cast<int>(mAnimTime * mFrameCount);
    }
    else {
        frameIndex = static_cast<int>(mAnimTime * (mFrameCount - 1));
    }

    // ȷ��֡��������Ч��Χ��
    frameIndex = std::max(0, std::min(frameIndex, mFrameCount - 1));
    return frameIndex;
}

// ====================================================================================================
// ReanimationHolder ʵ��
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
    for (auto it = mReanimations.begin(); it != mReanimations.end(); ) {
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
    // ����Ⱦ˳�����������Ҫ��
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
// ȫ�ֺ���ʵ��
// ====================================================================================================

void ReanimatorLoadDefinitions() {
    // �������Ԥ���س��õĶ�������
    TOD_TRACE("Reanimator definitions loaded");
}

void ReanimatorFreeDefinitions() {
    // ����ȫ�ֶ���������Դ
    TOD_TRACE("Reanimator definitions freed");
}

void ReanimationPreload(ReanimationType theReanimationType) {
    // Ԥ�����ض����͵Ķ���
    TOD_TRACE("Preloaded reanimation type: " + std::to_string(static_cast<int>(theReanimationType)));
}