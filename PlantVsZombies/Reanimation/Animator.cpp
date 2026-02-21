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
                for (int i = 0; i < static_cast<int>(reanim->GetTrackCount()); i++) {
                    auto track = reanim->GetTrack(i);
                    if (track) {
                        mTrackIndicesMap[track->mTrackName] = i;
                        TrackExtraInfo extra; 
                        mExtraInfos.push_back(extra);
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

void Animator::Draw(SDL_Renderer* renderer, float baseX, float baseY, float Scale) {
    if (!mReanim || !renderer) return;

    // 保存渲染器混合模式
    SDL_BlendMode oldRenderBlend;
    SDL_GetRenderDrawBlendMode(renderer, &oldRenderBlend);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // 收集所有绘制命令
    std::vector<AnimDrawCommand> commands;
    CollectDrawCommands(commands, baseX, baseY, Scale);

    // 保存所有用到的纹理的原始状态
    std::map<SDL_Texture*, std::tuple<SDL_BlendMode, Uint8, Uint8, Uint8, Uint8>> originalStates;
    for (const auto& cmd : commands) {
        if (originalStates.find(cmd.texture) == originalStates.end()) {
            SDL_BlendMode blend;
            Uint8 r, g, b, a;
            SDL_GetTextureBlendMode(cmd.texture, &blend);
            SDL_GetTextureColorMod(cmd.texture, &r, &g, &b);
            SDL_GetTextureAlphaMod(cmd.texture, &a);
            originalStates[cmd.texture] = { blend, r, g, b, a };
        }
    }

    // 顺序扫描命令，合并连续相同纹理+混合模式的四边形
    size_t i = 0;
    while (i < commands.size()) {
        SDL_Texture* currentTexture = commands[i].texture;
        SDL_BlendMode currentBlend = commands[i].blendMode;

        // 设置当前批次的纹理状态（颜色调制由顶点控制）
        SDL_SetTextureBlendMode(currentTexture, currentBlend);
        SDL_SetTextureColorMod(currentTexture, 255, 255, 255);
        SDL_SetTextureAlphaMod(currentTexture, 255);

        std::vector<SDL_Vertex> vertices;
        std::vector<int> indices;
        int baseVertex = 0;

        // 收集所有连续的相同纹理+混合模式的命令
        while (i < commands.size() &&
            commands[i].texture == currentTexture &&
            commands[i].blendMode == currentBlend) {
            const auto& cmd = commands[i];
            for (int v = 0; v < 4; ++v) {
                SDL_Vertex vertex;
                vertex.position.x = cmd.points[v * 2];
                vertex.position.y = cmd.points[v * 2 + 1];
                vertex.color = cmd.color;
                vertex.tex_coord.x = (v == 0 || v == 3) ? 0.0f : 1.0f;
                vertex.tex_coord.y = (v == 0 || v == 1) ? 0.0f : 1.0f;
                vertices.push_back(vertex);
            }
            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 1);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 0);
            indices.push_back(baseVertex + 2);
            indices.push_back(baseVertex + 3);
            baseVertex += 4;
            ++i;
        }

        // 绘制当前批次
        if (!vertices.empty()) {
            SDL_RenderGeometry(renderer, currentTexture, vertices.data(), static_cast<int>(vertices.size()),
                indices.data(), static_cast<int>(indices.size()));
        }
    }

    // 恢复所有纹理的原始状态
    for (const auto& [texture, state] : originalStates) {
        auto [blend, r, g, b, a] = state;
        SDL_SetTextureBlendMode(texture, blend);
        SDL_SetTextureColorMod(texture, r, g, b);
        SDL_SetTextureAlphaMod(texture, a);
    }

    SDL_SetRenderDrawBlendMode(renderer, oldRenderBlend);
}

void Animator::DrawTexturedQuad(SDL_Renderer* renderer, SDL_Texture* texture,
    const float points[8], SDL_Color color)
{
    SDL_Vertex vertices[4];
    for (int i = 0; i < 4; ++i) {
        vertices[i].position.x = points[i * 2];
        vertices[i].position.y = points[i * 2 + 1];
        vertices[i].color = color;
        vertices[i].tex_coord.x = (i == 0 || i == 3) ? 0.0f : 1.0f;
        vertices[i].tex_coord.y = (i == 0 || i == 1) ? 0.0f : 1.0f;
    }
    int indices[6] = { 0, 1, 2, 0, 2, 3 };
    SDL_RenderGeometry(renderer, texture, vertices, 4, indices, 6);
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
        SDL_Texture* image = nullptr;

        if (shouldDrawSelf) {
            image = mExtraInfos[i].mImage ? mExtraInfos[i].mImage : transform.image;
            shouldDrawSelf = (image != nullptr);
        }

        if (shouldDrawSelf) {
            int imgWidth, imgHeight;
            SDL_QueryTexture(image, NULL, NULL, &imgWidth, &imgHeight);
            float w = static_cast<float>(imgWidth);
            float h = static_cast<float>(imgHeight);

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
            Uint8 baseAlpha = static_cast<Uint8>(std::clamp(combinedAlpha * 255.0f, 0.0f, 255.0f));

            // 正常绘制
            AnimDrawCommand cmd;
            cmd.texture = image;
            cmd.blendMode = SDL_BLENDMODE_BLEND;
            cmd.color = { 255, 255, 255, baseAlpha };
            memcpy(cmd.points, worldPoints, sizeof(worldPoints));
            outCommands.push_back(cmd);

            // 发光效果
            if (mEnableExtraAdditiveDraw) {
                cmd.blendMode = SDL_BLENDMODE_ADD;
                cmd.color = mExtraAdditiveColor;
                outCommands.push_back(cmd);
            }

            // 覆盖层效果
            if (mEnableExtraOverlayDraw) {
                cmd.blendMode = SDL_BLENDMODE_BLEND;
                cmd.color = mExtraOverlayColor;
                cmd.color.a = ColorComponentMultiply(mExtraOverlayColor.a, baseAlpha);
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