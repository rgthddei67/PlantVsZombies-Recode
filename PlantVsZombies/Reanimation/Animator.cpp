#include "Animator.h"
#include "../DeltaTime.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/matrix_decompose.hpp>

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

void Animator::TrackAttachAnimatorMatrix(const std::string& trackName, glm::mat4* target) {
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

	// 保存当前渲染器状态
	SDL_BlendMode oldBlendMode;
	SDL_GetRenderDrawBlendMode(renderer, &oldBlendMode);
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
		if (transform.f != 0) continue;

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

		// 计算精确的浮点数位置（左上角）
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

		// 创建浮点数矩形（左上角基准）
		SDL_FRect dstRect = {
			exactPosX,
			exactPosY,
			scaledWidth,
			scaledHeight
		};

		// 计算基础透明度（原版逻辑）
		float combinedAlpha = transform.a * mAlpha;
		Uint8 baseAlpha = static_cast<Uint8>(std::clamp(combinedAlpha * 255.0f, 0.0f, 255.0f));

		// 旋转中心点为(0,0)（左上角）
		SDL_FPoint center = { 0.0f, 0.0f };

		// 第一步：正常绘制
		SDL_SetTextureAlphaMod(image, baseAlpha);
		SDL_SetTextureColorMod(image, 255, 255, 255);
		SDL_RenderCopyExF(renderer, image, NULL, &dstRect,
			transform.kx, &center, SDL_FLIP_NONE);

		// 第二步：原版PVZ的发光效果实现
		if (mEnableExtraAdditiveDraw) {
			SDL_BlendMode textureBlendMode;
			SDL_GetTextureBlendMode(image, &textureBlendMode);

			// 设置纹理为加法混合模式
			SDL_SetTextureBlendMode(image, SDL_BLENDMODE_ADD);

			// 设置颜色和透明度
			SDL_SetTextureColorMod(image, mExtraAdditiveColor.r,
				mExtraAdditiveColor.g,
				mExtraAdditiveColor.b);
			SDL_SetTextureAlphaMod(image, mExtraAdditiveColor.a);

			// 绘制
			SDL_RenderCopyExF(renderer, image, NULL, &dstRect,
				transform.kx, &center, SDL_FLIP_NONE);

			// 恢复纹理混合模式
			SDL_SetTextureBlendMode(image, textureBlendMode);

			// 重置纹理颜色
			SDL_SetTextureColorMod(image, 255, 255, 255);
			SDL_SetTextureAlphaMod(image, 255);
		}

		// 第三步：覆盖层（原版PVZ的覆盖层）
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

glm::mat4 Animator::CalculateTransformMatrix(const TrackFrameTransform& tf, const TrackExtraInfo& extra, float Scale) const {
	glm::mat4 result(1.0f);

	TransformToMatrix(tf, result, Scale, Scale, extra.mOffsetX, extra.mOffsetY);

	return result;
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