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

Animator::~Animator()
{
    Die();
}

void Animator::Die() {
    // 先让所有附加的子动画死亡
    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
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
                mTrackIndicesMap[track->mTrackName] = i;
                TrackExtraInfo extra;

                // 预加载该轨道用到的所有图片
                for (size_t i = 0; i < track->mFrames.size(); i++) {
                    const auto frame = track->mFrames[i];
                    if (!frame.image.empty()) {
                        // 如果缓存中还没有该图片，则加载
                        if (extra.mTextureCache.find(frame.image) == extra.mTextureCache.end()) {
                            SDL_Texture* tex = reanim->mResourceManager->GetTexture(frame.image);
                            extra.mTextureCache[frame.image] = tex;
                        }
                    }
				}

                mExtraInfos.push_back(extra);
            }
        }

        mPlayingState = PlayState::PLAY_REPEAT;
        mIsPlaying = false;
        mTargetTrack = "";
    }
}

void Animator::AddFrameEvent(int frameIndex, std::function<void()> callback) {
    if (callback) {
        mFrameEvents.insert({ frameIndex, callback });
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
        SetSpeed(speed);
    }

    mIsPlaying = true;
    mPlayingState = PlayState::PLAY_REPEAT;

    return true;
}

bool Animator::PlayTrackOnce(const std::string& trackName, const std::string& returnTrack,
    float speed, float blendTime)
{
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

    // 帧索引前进
    float frameAdvance = deltaTime * mFPS * mSpeed;
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
                PlayTrack(mTargetTrack, mOriginalSpeed, 0.5f);
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

    // ----- 触发帧事件（一次性，触发后自动移除）-----
    int oldInt = static_cast<int>(oldFrame);
    int newInt = static_cast<int>(mFrameIndexNow);

    if (newInt >= oldInt) {
        // 正常前进或不变
        for (int f = oldInt + 1; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second();               // 执行回调
                it = mFrameEvents.erase(it); // 移除（一次性）
            }
        }
    }
    else {
        // 发生了回绕（循环播放）
        int endInt = static_cast<int>(mFrameIndexEnd);
        for (int f = oldInt + 1; f <= endInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second();
                it = mFrameEvents.erase(it);
            }
        }
        int beginInt = static_cast<int>(mFrameIndexBegin);
        for (int f = beginInt; f <= newInt; ++f) {
            auto range = mFrameEvents.equal_range(f);
            for (auto it = range.first; it != range.second;) {
                it->second();
                it = mFrameEvents.erase(it);
            }
        }
    }

    // 更新混合计时器
    if (mReanimBlendCounter > 0) {
        mReanimBlendCounter -= deltaTime;
        if (mReanimBlendCounter < 0) mReanimBlendCounter = 0;
    }

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->Update();
            }
		}
    }
}

void Animator::SetSpeed(float speed)
{
    this->mSpeed = speed;

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetSpeed(speed);
            }
        }
	}
}

void Animator::SetAlpha(float alpha)
{
    this->mAlpha = alpha;

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetAlpha(alpha);
            }
        }
	}
}

void Animator::SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    this->mExtraAdditiveColor = { r, g, b, a };

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetGlowColor(r, g, b, a);
            }
        }
	}
}

void Animator::SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a)
{
    this->mExtraOverlayColor = { r, g, b, a };

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->SetOverlayColor(r, g, b, a);
            }
        }
    }
}

void Animator::EnableGlowEffect(bool enable)
{
    this->mEnableExtraAdditiveDraw = enable;

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->EnableGlowEffect(enable);
            }
        }
	}
}

void Animator::EnableOverlayEffect(bool enable)
{
    this->mEnableExtraOverlayDraw = enable;

    for (size_t i = 0; i < mExtraInfos.size(); i++)
    {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++)
        {
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

void Animator::SetTrackImage(const std::string& trackName, SDL_Texture* image) {
    for (auto& extra : GetTrackExtrasByName(trackName)) {
        extra->mImage = image;
    }
}

void Animator::SetLocalPosition(float x, float y) {
    mLocalPosX = x;
    mLocalPosY = y;
}

void Animator::SetLocalPosition(const Vector& position)
{
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
    // std::cout << "GetTrackRange: track '" << trackName << "' has " << totalFrames << " frames." << std::endl;

    int start = -1;
    for (int i = 0; i < totalFrames; ++i) {
        if (track->mFrames[i].f == 0) {
            start = i;
            // std::cout << "GetTrackRange: first f=0 at index " << i << std::endl;
            break;
        }
    }

    if (start == -1) {
        std::cout << "GetTrackRange: no f=0 frames, returning invalid." << std::endl;
        return { -1, -1 };   // 改为返回无效范围
    }

    int end = start;
    for (int i = start + 1; i < totalFrames; ++i) {
        if (track->mFrames[i].f == 0) {
            end = i;
            // std::cout << "GetTrackRange: continue f=0 at " << i << ", end now " << end << std::endl;
        }
        else if (track->mFrames[i].f == -1) {
            // std::cout << "GetTrackRange: f=-1 at " << i << ", stopping. end = " << end << std::endl;
            break;
        }
        else {
            std::cout << "GetTrackRange: unexpected f=" << track->mFrames[i].f << " at " << i << ", stopping." << std::endl;
            break;
        }
    }

    // std::cout << "GetTrackRange: returning start=" << start << " end=" << end << std::endl;
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

void Animator::Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale) {
    if (!mReanim || !renderer) return;

    SDL_BlendMode oldBlendMode;
    SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    for (int i = 0; i < mReanim->GetTrackCount(); i++) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        // 1. 先获取插值变换（即使当前帧是隐藏帧，也能得到变换数据，用于子动画定位）
        TrackFrameTransform transform = GetInterpolatedTransform(i);
        // transform.f 可能为 -1，但其他属性（位置、缩放、旋转）有效

        // 2. 轨道自身绘制条件：轨道可见、非隐藏帧、且有纹理
        bool shouldDrawSelf = (i < static_cast<int>(mExtraInfos.size()) &&
            mExtraInfos[i].mVisible &&
            transform.f != -1);
        SDL_Texture* image = nullptr;

        if (shouldDrawSelf) {
            if (mExtraInfos[i].mImage) {
                image = mExtraInfos[i].mImage; // 手动设置的纹理优先级最高
            }
            else if (!transform.image.empty()) {
                auto it = mExtraInfos[i].mTextureCache.find(transform.image);
                if (it != mExtraInfos[i].mTextureCache.end()) {
                    image = it->second;
                }
                // 若没有，再加载
                else if (mReanim->mResourceManager) {
                    image = mReanim->mResourceManager->GetTexture(transform.image);
                    if (image) mExtraInfos[i].mTextureCache[transform.image] = image;
                }
            }
            shouldDrawSelf = (image != nullptr);
        }

        // 3. 绘制轨道自身
        if (shouldDrawSelf) {
            int imgWidth, imgHeight;
            SDL_QueryTexture(image, NULL, NULL, &imgWidth, &imgHeight);

            float worldX = baseX + (transform.x + mExtraInfos[i].mOffsetX) * Scale;
            float worldY = baseY + (transform.y + mExtraInfos[i].mOffsetY) * Scale;
            float worldScaleX = transform.sx * Scale;
            float worldScaleY = transform.sy * Scale;

            float scaledWidth = imgWidth * worldScaleX;
            float scaledHeight = imgHeight * worldScaleY;
            if (scaledWidth < 1.0f) scaledWidth = 1.0f;
            if (scaledHeight < 1.0f) scaledHeight = 1.0f;

            SDL_FRect dstRect = { worldX, worldY, scaledWidth, scaledHeight };
            SDL_FPoint center = { 0.0f, 0.0f };

            float combinedAlpha = transform.a * mAlpha;
            Uint8 baseAlpha = static_cast<Uint8>(std::clamp(combinedAlpha * 255.0f, 0.0f, 255.0f));

            // 正常绘制
            SDL_SetTextureAlphaMod(image, baseAlpha);
            SDL_SetTextureColorMod(image, 255, 255, 255);
            SDL_RenderCopyExF(renderer, image, NULL, &dstRect, transform.kx, &center, SDL_FLIP_NONE);

            // 发光效果
            if (mEnableExtraAdditiveDraw) {
                SDL_BlendMode textureBlendMode;
                SDL_GetTextureBlendMode(image, &textureBlendMode);
                SDL_SetTextureBlendMode(image, SDL_BLENDMODE_ADD);
                SDL_SetTextureColorMod(image, mExtraAdditiveColor.r, mExtraAdditiveColor.g, mExtraAdditiveColor.b);
                SDL_SetTextureAlphaMod(image, mExtraAdditiveColor.a);
                SDL_RenderCopyExF(renderer, image, NULL, &dstRect, transform.kx, &center, SDL_FLIP_NONE);
                SDL_SetTextureBlendMode(image, textureBlendMode);
                SDL_SetTextureColorMod(image, 255, 255, 255);
                SDL_SetTextureAlphaMod(image, 255);
            }

            // 覆盖层效果
            if (mEnableExtraOverlayDraw) {
                SDL_Color overlayColor = mExtraOverlayColor;
                overlayColor.a = ColorComponentMultiply(mExtraOverlayColor.a, baseAlpha);
                SDL_SetTextureColorMod(image, overlayColor.r, overlayColor.g, overlayColor.b);
                SDL_SetTextureAlphaMod(image, overlayColor.a);
                SDL_RenderCopyExF(renderer, image, NULL, &dstRect, transform.kx, &center, SDL_FLIP_NONE);
                SDL_SetTextureColorMod(image, 255, 255, 255);
                SDL_SetTextureAlphaMod(image, 255);
            }
        }

        // 4. 绘制附加到该轨道的子 Animator
        if (i < static_cast<int>(mExtraInfos.size())) {
            for (const auto& weakChild : mExtraInfos[i].mAttachedReanims) {
                if (auto child = weakChild.lock()) {
                    if (!child->mReanim) continue;

                    // 计算子动画的世界位置：父轨道位置 + 父轨道偏移 + 子本地偏移（考虑父缩放）
                    float childWorldX = baseX + (transform.x + mExtraInfos[i].mOffsetX) * Scale
                        + child->mLocalPosX * (transform.sx * Scale);
                    float childWorldY = baseY + (transform.y + mExtraInfos[i].mOffsetY) * Scale
                        + child->mLocalPosY * (transform.sy * Scale);

                    // 调用子动画的绘制，Scale 参数传 1.0f，因为子动画内部会使用自己的 mLocalScale       
                    child->Draw(renderer, childWorldX, childWorldY, 1.0f);
                }
            }
        }
    }

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