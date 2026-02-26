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
                mTrackIndicesMap[track->mTrackName] = i;
                TrackExtraInfo extra;
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

    for (size_t i = 0; i < mExtraInfos.size(); i++) {
        for (size_t j = 0; j < mExtraInfos[i].mAttachedReanims.size(); j++) {
            auto child = mExtraInfos[i].mAttachedReanims[j].lock();
            if (child) {
                child->Update();
            }
        }
    }
}

void Animator::PrepareCommands(float baseX, float baseY, float Scale) {
    mCachedCommands.clear();
    if (!mReanim) return;
    CollectDrawCommands(mCachedCommands, baseX, baseY, Scale);
}

void Animator::Draw(Graphics* g, float baseX, float baseY, float Scale) {
    if (!mReanim || !g) return;

    // 优先使用 PrepareCommands() 预计算的缓存（多线程模式）
    // 若缓存为空则即时计算（单线程回退）
    std::vector<AnimDrawCommand> localCommands;
    const std::vector<AnimDrawCommand>* commands;

    if (!mCachedCommands.empty()) {
        commands = &mCachedCommands;
    }
    else {
        CollectDrawCommands(localCommands, baseX, baseY, Scale);
        commands = &localCommands;
    }

    // 保存当前变换栈，确保我们不叠加额外变换
    g->PushTransform(glm::mat4(1.0f));

    BlendMode originalBlend = g->GetBlendMode();  // 记录进入时的混合模式
    BlendMode lastBlend = originalBlend;

    for (const auto& cmd : *commands) {
        // 切换混合模式（若不同）
        if (cmd.blendMode != lastBlend) {
            g->SetBlendMode(cmd.blendMode);
            lastBlend = cmd.blendMode;
        }

        // 从 points 提取四个顶点坐标（顺序：左上、右上、右下、左下）
        float x0 = cmd.points[0], y0 = cmd.points[1];
        float x1 = cmd.points[2], y1 = cmd.points[3];
        float x2 = cmd.points[4], y2 = cmd.points[5];
        float x3 = cmd.points[6], y3 = cmd.points[7];

        // 构造仿射变换矩阵（列主序，将单位矩形映射到目标四边形）
        glm::mat4 transform(
            x1 - x0, y1 - y0, 0.0f, 0.0f,   // 第一列 (a, d, 0, 0)
            x3 - x0, y3 - y0, 0.0f, 0.0f,   // 第二列 (b, e, 0, 0)
            0.0f, 0.0f, 1.0f, 0.0f,         // 第三列 (0,0,1,0)
            x0, y0, 0.0f, 1.0f              // 第四列 (tx, ty, 0, 1)
        );

        g->DrawTextureMatrix(cmd.texture, transform, 0.0f, 0.0f, cmd.color);
    }

    // 恢复进入时的混合模式
    if (g->GetBlendMode() != originalBlend) {
        g->SetBlendMode(originalBlend);
    }

    g->PopTransform();

    // 清空缓存，下帧由 PrepareCommands() 重新填充
    mCachedCommands.clear();
}

void Animator::CollectDrawCommands(std::vector<AnimDrawCommand>& outCommands, float baseX, float baseY, float Scale) const {
    if (!mReanim) return;

    const float DEG_TO_RAD = 3.14159265358979323846f / 180.0f;

    for (int i = 0; i < static_cast<int>(mReanim->GetTrackCount()); ++i) {
        auto track = mReanim->GetTrack(i);
        if (!track || !track->mAvailable || track->mFrames.empty()) continue;

        TrackFrameTransform transform = GetInterpolatedTransform(i);

        bool shouldDrawSelf = (i < static_cast<int>(mExtraInfos.size()) &&
            mExtraInfos[i].mVisible &&
            transform.f != -1);
        const GLTexture* image = nullptr;

        if (shouldDrawSelf) {
            image = mExtraInfos[i].mImage ? mExtraInfos[i].mImage : transform.image;
            shouldDrawSelf = (image != nullptr);
        }

        if (shouldDrawSelf) {
            // 获取纹理原始尺寸（假设 GLTexture 包含 width/height 成员）
            int imgWidth = image->width;
            int imgHeight = image->height;
            float w = static_cast<float>(imgWidth);
            float h = static_cast<float>(imgHeight);

            // 计算扭曲参数（与原逻辑相同）
            float angleX = -transform.kx * DEG_TO_RAD;
            float angleY = -transform.ky * DEG_TO_RAD;
            float cosX = cosf(angleX);
            float sinX = sinf(angleX);
            float cosY = cosf(angleY);
            float sinY = sinf(angleY);

            float a = cosX * transform.sx;
            float b = -sinX * transform.sx;
            float c = sinY * transform.sy;
            float d = cosY * transform.sy;

            float tx = transform.x + mExtraInfos[i].mOffsetX;
            float ty = transform.y + mExtraInfos[i].mOffsetY;

            float worldPoints[8];
            for (int j = 0; j < 4; ++j) {
                float u = (j == 0 || j == 3) ? 0.0f : w;
                float v = (j == 0 || j == 1) ? 0.0f : h;

                float x = a * u + c * v + tx;
                float y = b * u + d * v + ty;

                worldPoints[j * 2] = baseX + x * Scale;
                worldPoints[j * 2 + 1] = baseY + y * Scale;
            }

            float combinedAlpha = transform.a * mAlpha;
            float baseAlpha = std::clamp(combinedAlpha, 0.0f, 1.0f);

            // 正常绘制命令
            AnimDrawCommand cmd;
            cmd.texture = image;
            cmd.blendMode = BlendMode::Alpha;
            cmd.color = glm::vec4(1.0f, 1.0f, 1.0f, baseAlpha);
            memcpy(cmd.points, worldPoints, sizeof(worldPoints));
            outCommands.push_back(cmd);

            // 发光效果（叠加混合）
            if (mEnableExtraAdditiveDraw) {
                cmd.blendMode = BlendMode::Add;
                cmd.color = glm::vec4(mExtraAdditiveColor.r / 255.0f,
                    mExtraAdditiveColor.g / 255.0f,
                    mExtraAdditiveColor.b / 255.0f,
                    mExtraAdditiveColor.a / 255.0f);
                outCommands.push_back(cmd);
            }

            // 覆盖层效果（Alpha 混合，颜色需乘以基础透明度）
            if (mEnableExtraOverlayDraw) {
                cmd.blendMode = BlendMode::Alpha;
                glm::vec4 overlayColor(mExtraOverlayColor.r / 255.0f,
                    mExtraOverlayColor.g / 255.0f,
                    mExtraOverlayColor.b / 255.0f,
                    (mExtraOverlayColor.a / 255.0f) * baseAlpha);
                cmd.color = overlayColor;
                outCommands.push_back(cmd);
            }
        }

        // 子动画
        if (i < static_cast<int>(mExtraInfos.size())) {
            for (const auto& weakChild : mExtraInfos[i].mAttachedReanims) {
                auto child = weakChild.lock();
                if (!child || !child->mReanim) continue;

                float angleX = -transform.kx * DEG_TO_RAD;
                float angleY = -transform.ky * DEG_TO_RAD;
                float cosX = cosf(angleX), sinX = sinf(angleX);
                float cosY = cosf(angleY), sinY = sinf(angleY);
                float a = cosX * transform.sx;
                float b = -sinX * transform.sx;
                float c = sinY * transform.sy;
                float d = cosY * transform.sy;

                float tx = transform.x + mExtraInfos[i].mOffsetX;
                float ty = transform.y + mExtraInfos[i].mOffsetY;

                float childX = child->mLocalPosX;
                float childY = child->mLocalPosY;

                float worldX = tx + a * childX + c * childY;
                float worldY = ty + b * childX + d * childY;

                worldX = baseX + worldX * Scale;
                worldY = baseY + worldY * Scale;

                child->CollectDrawCommands(outCommands, worldX, worldY, 1.0f);
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

void Animator::SetTrackImage(const std::string& trackName, const GLTexture* image) {
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
    if (!mReanim) return 0.0f;

    // 获取轨道
    auto tracks = GetTracksByName(trackName);
    if (tracks.empty()) return 0.0f;
    TrackInfo* track = tracks[0];
    if (!track || track->mFrames.empty()) return 0.0f;

    // 获取当前帧整数索引，并限制在有效范围内
    int frameBefore = static_cast<int>(mFrameIndexNow);
    int maxIndex = static_cast<int>(track->mFrames.size()) - 1;
    frameBefore = std::clamp(frameBefore, 0, maxIndex);
    int frameAfter = std::min(frameBefore + 1, maxIndex);

    // 获取前后帧的 X 坐标
    float xBefore = track->mFrames[frameBefore].x;
    float xAfter = track->mFrames[frameAfter].x;
    float dx = xAfter - xBefore;

    // 计算速度
    float velocity = dx * mSpeed;
    return std::abs(velocity);
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