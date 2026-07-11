#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include "../Graphics.h"
#include "../Game/DeferredEvent.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
#include <vector>
#include <glm/glm.hpp>
#include <iostream>

/**
 * @brief 颜色分量乘法 (通道值范围 0-255)
 * @param theColor1 颜色1分量
 * @param theColor2 颜色2分量
 * @return 相乘后的分量 (clamp to 0-255)
 */
int ColorComponentMultiply(int theColor1, int theColor2);

/**
 * @brief SDL_Color 颜色乘法 (逐分量相乘)
 * @param theColor1 颜色1
 * @param theColor2 颜色2
 * @return 相乘后的颜色
 */
SDL_Color ColorsMultiply(const SDL_Color& theColor1, const SDL_Color& theColor2);

class Animator {
private:
	std::shared_ptr<Reanimation> mReanim;   ///< 关联的 Reanimation 数据
	float mFPS = 12.0f;                      ///< 帧率
	float mFrameIndexNow = 0.0f;             ///< 当前帧索引 (浮点，支持插值)
	float mFrameIndexBegin = 0.0f;           ///< 起始帧索引
	float mFrameIndexEnd = 0.0f;              ///< 结束帧索引
	float mSpeed = 1.0f;                      ///< 基础播放速度 (每实例随机的 base，PlayTrack 不再覆盖它)
	float mClipSpeed = 0.0f;                  ///< 当前轨道的绝对速度覆盖；0 = 回落到 mSpeed。每次 PlayTrack 重设，随轨道作用域
	float mExtraSpeedMultiplier = 1.0f;       ///< 额外速度倍率 (正交状态层，减速/冻结，跨 PlayTrack 存活)
	float mAlpha = 1.0f;                      ///< 整体透明度

	// 过渡动画相关
	float mReanimBlendCounter = -1.0f;        ///< 混合计数器，>0 时进行混合
	float mReanimBlendCounterMax = 100.0f;     ///< 混合总时长 (秒)
	int mFrameIndexBlendBuffer = 0;            ///< 混合起始帧索引

	bool mIsPlaying = false;                   ///< 是否正在播放
	PlayState mPlayingState = PlayState::PLAY_NONE;  ///< 播放状态
	std::vector<TrackExtraInfo> mExtraInfos;   ///< 每个轨道的额外信息 (可见性、自定义纹理、附加子动画等)
	std::unordered_map<std::string, int> mTrackIndicesMap;  ///< 轨道名称到索引的映射

	bool mEnableExtraAdditiveDraw = false;     ///< 是否启用高亮 (叠加混合) 效果
	bool mEnableExtraOverlayDraw = false;      ///< 是否启用附加覆盖效果
	SDL_Color mExtraAdditiveColor = { 255, 255, 255, 128 };  ///< 高亮颜色
	SDL_Color mExtraOverlayColor = { 255, 255, 255, 64 };    ///< 覆盖层颜色

	std::string mCurrentTrackName = "";         ///< 当前正在播放的轨道名

	// 过渡目标
	std::string mTargetTrack = "";              ///< 播放一次后要切换到的轨道名
	float mTargetTrackSpeed = 0.0f;             ///< 回切到 mTargetTrack 时用的 clip 速度（0=回落 base），由 PlayTrackOnce 指定

	struct FrameEvent {
		std::function<void()> callback;
		bool persistent;   ///< false=一次性，触发后移除；true=每次穿过该帧都触发
	};
	std::unordered_multimap<int, FrameEvent> mFrameEvents;  ///< 帧事件表

public:
	/**
	 * @brief 默认构造函数
	 */
	Animator();

	/**
	 * @brief 构造函数，绑定 Reanimation
	 * @param reanim 要绑定的 Reanimation 共享指针
	 */
	explicit Animator(std::shared_ptr<Reanimation> reanim);

	/**
	 * @brief 析构函数，释放资源并清理子动画
	 */
	~Animator();

	/**
	 * @brief 初始化，绑定 Reanimation
	 * @param reanim 要绑定的 Reanimation 共享指针
	 */
	void Init(std::shared_ptr<Reanimation> reanim);

	/**
	 * @brief 销毁动画器，停止播放并清除所有子动画和事件
	 */
	void Die();

	// ---------- 播放控制 ----------
	/**
	 * @brief 开始播放
	 * @param state 播放状态 (默认重复播放)
	 */
	void Play(PlayState state = PlayState::PLAY_REPEAT);

	/**
	 * @brief 暂停播放
	 */
	void Pause();

	/**
	 * @brief 停止播放，并将当前帧重置为起始帧
	 */
	void Stop();

	/**
	 * @brief 添加帧事件
	 * @param frameIndex 帧索引 (整数)
	 * @param callback 回调函数
	 * @param persistent false=一次性，触发后自动移除（默认）；true=持久事件，每次帧索引穿过该帧都触发，仅在 Die()/Init() 时清空
	 */
	void AddFrameEvent(int frameIndex, std::function<void()> callback, bool persistent = false);

	/**
	 * @brief 播放指定轨道动画，支持过渡效果
	 * @param trackName 轨道名
	 * @param speed 轨道绝对播放速度，0.0=回落到基础速度(base)，>0=本轨道固定用该速度
	 * @param blendTime 过渡时间 (秒)，0表示无过渡
	 * @return 是否成功
	 */
	bool PlayTrack(const std::string& trackName, float speed = 0.0f, float blendTime = 0);

	/**
	 * @brief 播放指定轨道动画一次，播放完后可切换回另一轨道
	 * @param trackName 要播放的轨道名
	 * @param returnTrack 播放完后要返回的轨道名 (为空则不切换)
	 * @param speed 本轨道绝对播放速度，0.0=回落到基础速度(base)，>0=本轨道固定用该速度
	 * @param blendTime 过渡时间
	 * @param returnSpeed 回切到 returnTrack 时的 clip 速度，0.0=回落 base，>0=回切轨道固定用该速度
	 * @return 是否成功
	 */
	bool PlayTrackOnce(const std::string& trackName,
		const std::string& returnTrack = "",
		float speed = 0.0f,
		float blendTime = 0,
		float returnSpeed = 0.0f);


	// ---------- 轨道范围控制 ----------
	/**
	 * @brief 获取轨道对应的帧范围 (根据 f==0 的帧划分)
	 * @param trackName 轨道名
	 * @return pair {起始帧索引, 结束帧索引}，若无效返回 {-1,-1}
	 */
	std::pair<int, int> GetTrackRange(const std::string& trackName);

	/**
	 * @brief 手动设置帧范围
	 * @param frameBegin 起始帧索引
	 * @param frameEnd 结束帧索引
	 */
	void SetFrameRange(int frameBegin, int frameEnd);

	/**
	 * @brief 根据轨道名设置帧范围 (自动调用 GetTrackRange)
	 * @param trackName 轨道名
	 */
	void SetFrameRangeByTrackName(const std::string& trackName);

	/**
	 * @brief 将帧范围恢复为整个动画的范围 (0 到 总帧数-1)
	 */
	void SetFrameRangeToDefault();

	/**
	 * @brief 设置轨道自定义纹理 (会覆盖动画本身的纹理)
	 * @param trackName 轨道名
	 * @param image 纹理指针，nullptr 表示恢复默认
	 */
	void SetTrackImage(const std::string& trackName, const Texture* image);

	/**
	 * @brief 设置轨道可见性
	 * @param trackName 轨道名
	 * @param visible true=显示，false=隐藏
	 */
	void SetTrackVisible(const std::string& trackName, bool visible);

	/**
	 * @brief 将子动画器附加到指定轨道 (子动画将跟随父轨道的变换)
	 * @param trackName 目标轨道名
	 * @param child 子动画器共享指针
	 * @return 是否成功 (轨道存在且不为自身)
	 */
	bool AttachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);

	/**
	 * @brief 从指定轨道分离子动画器
	 * @param trackName 轨道名
	 * @param child 子动画器共享指针
	 */
	void DetachAnimator(const std::string& trackName, std::shared_ptr<Animator> child);

	/**
	 * @brief 分离所有轨道的所有子动画器
	 */
	void DetachAllAnimators();

	// ---------- 状态查询 ----------
	/**
	 * @brief 是否正在播放
	 */
	bool IsPlaying() const { return mIsPlaying; }

	/**
	 * @brief 获取当前帧索引 (浮点)
	 */
	float GetCurrentFrame() const { return mFrameIndexNow; }

	/**
	 * @brief 获取当前正在播放的轨道名
	 */
	const std::string& GetCurrentTrackName() const { return mCurrentTrackName; }

	/**
	 * @brief 获取当前播放状态 (循环 / 一次性 / 一次性后切换)。
	 *        存档必须持久化它，否则读档统一走 PlayTrack 会把进行中的一次性轨道
	 *        误当 PLAY_REPEAT 永远循环、无法切回目标轨道。
	 */
	PlayState GetPlayingState() const { return mPlayingState; }

	/**
	 * @brief 获取 PlayTrackOnce 播完后要切换到的目标轨道名 (空=播完即停，不切换)
	 */
	const std::string& GetTargetTrack() const { return mTargetTrack; }

	/**
	 * @brief 获取回切到目标轨道时使用的 clip 速度 (0=回落 base)
	 */
	float GetTargetTrackSpeed() const { return mTargetTrackSpeed; }

	/**
	 * @brief 直接设置当前帧索引 (用于存档恢复)
	 */
	void SetCurrentFrame(float frameIndex) { mFrameIndexNow = frameIndex; }

	/**
	 * @brief 设置播放速度倍率
	 * @param speed 速度倍率
	 */
	void SetSpeed(float speed);

	/**
	 * @brief 获取播放速度倍率
	 */
	float GetSpeed() const { return mSpeed; }

	/**
	 * @brief 设置当前轨道的绝对速度覆盖 (0 = 回落到基础速度 mSpeed)。
	 *        会递归到所有附加子动画，复刻旧 SetSpeed 的递归语义。
	 */
	void SetClipSpeed(float clipSpeed);

	/**
	 * @brief 获取当前轨道速度覆盖值 (0 表示正使用基础速度)
	 */
	float GetClipSpeed() const { return mClipSpeed; }

	/**
	 * @brief 实际生效的播放速度 = (clip 覆盖优先, 否则 base) * 状态倍率
	 */
	float EffectiveSpeed() const {
		return (mClipSpeed != 0.0f ? mClipSpeed : mSpeed) * mExtraSpeedMultiplier;
	}

	/**
	 * @brief 设置额外速度倍率 (独立于 mSpeed，与 PlayTrack/SetSpeed 正交)
	 *        实际播放速度 = mSpeed * mExtraSpeedMultiplier，用于状态效果如减速
	 * @param mul 倍率
	 */
	void SetExtraSpeedMultiplier(float mul);

	/**
	 * @brief 获取额外速度倍率
	 */
	float GetExtraSpeedMultiplier() const { return mExtraSpeedMultiplier; }

	/**
	 * @brief 获取指定轨道的运动速度 (基于当前帧的前后位置差)
	 * @param trackName 轨道名
	 * @return 速度值 (像素/秒？实际为帧间位移乘以速度倍率)
	 */
	float GetTrackVelocity(const std::string& trackName) const;

	/**
	 * @brief 通过轨道索引获取运动速度 (跳过字符串查找，用于热路径)
	 * @param trackIndex 轨道索引
	 * @return 速度值
	 */
	float GetTrackVelocity(int trackIndex) const;

	/**
	 * @brief 根据轨道名获取第一个匹配的轨道索引 (O(1) 哈希查找)
	 * @param trackName 轨道名
	 * @return 索引，-1 表示未找到
	 */
	int GetFirstTrackIndexByName(const std::string& trackName) const;

	/**
	 * @brief 通过轨道名字获取一个track存不存在
	 * @param trackName 轨道名字
	 * @return 是否存在某个track，true=存在，false=不存在
	 */
	bool HasTrack(const std::string& trackName) const;

	// ---------- 透明度和颜色控制 ----------
	/**
	 * @brief 设置整体透明度
	 * @param alpha 透明度 (0~1)
	 */
	void SetAlpha(float alpha);

	/**
	 * @brief 获取整体透明度
	 */
	float GetAlpha() const { return mAlpha; }

	/**
	 * @brief 启用/禁用高亮 (叠加混合) 效果
	 * @param enable true=启用
	 */
	void EnableGlowEffect(bool enable);

	/**
	 * @brief 设置高亮颜色 (叠加混合)
	 * @param r, g, b, a 颜色分量 (0-255)
	 */
	void SetGlowColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 128);

	/**
	 * @brief 启用/禁用覆盖层效果 (Alpha混合)
	 * @param enable true=启用
	 */
	void EnableOverlayEffect(bool enable);

	/**
	 * @brief 设置覆盖层颜色
	 * @param r, g, b, a 颜色分量 (0-255)
	 */
	void SetOverlayColor(Uint8 r, Uint8 g, Uint8 b, Uint8 a = 64);

	// ---------- 更新和渲染 ----------
	/**
	 * @brief 更新动画逻辑 (帧前进、事件触发、子动画更新)
	 */
	void Update();

	/**
	 * @brief 阶段二并行段：完整推进帧 + 计时器 + 子节点递归；遇到帧事件 = 拷贝 callback 入 outBuf。
	 *        对象本地、worker 线程安全；不调用任何 callback。
	 *        前置：本 Animator 与其子 Animator 树不被其他线程并发访问（Task 1 audit PASS）。
	 */
	void UpdateParallelDeferred(std::vector<DeferredEvent>& outBuf);

	/**
	 * @brief 绘制动画 (现场计算变换并提交，递归绘制子动画)
	 * @param g Graphics 对象
	 * @param baseX 基准 X 坐标 (世界坐标)
	 * @param baseY 基准 Y 坐标
	 * @param Scale 全局缩放系数
	 */
	void Draw(Graphics* g, float baseX, float baseY, float Scale = 1.0f);

	/**
	 * @brief 获取底层 Reanimation 对象
	 */
	std::shared_ptr<Reanimation> GetReanimation() const { return mReanim; }

	// ---------- 轨道信息查询 ----------
	/**
	 * @brief 根据轨道名获取所有同名 TrackInfo 指针
	 * @param trackName 轨道名
	 * @return 指针数组
	 */
	std::vector<TrackInfo*> GetTracksByName(const std::string& trackName) const;

	/**
	 * @brief 获取轨道当前帧的位置
	 * @param trackName 轨道名
	 * @return 位置向量 (x,y)
	 */
	Vector GetTrackPosition(const std::string& trackName) const;

	/**
	 * @brief 获取轨道当前帧的旋转角度 (kx)
	 * @param trackName 轨道名
	 * @return 角度值
	 */
	float GetTrackRotation(const std::string& trackName) const;

	/**
	 * @brief 获取轨道的可见性
	 * @param trackName 轨道名
	 * @return true=可见
	 */
	bool GetTrackVisible(const std::string& trackName) const;

	// ---------- 本地变换 (用于子动画相对父级) ----------
	/**
	 * @brief 设置本地位置偏移
	 * @param x, y 偏移量
	 */
	void SetLocalPosition(float x, float y);

	/**
	 * @brief 设置本地位置偏移 (Vector 版本)
	 * @param position 偏移向量
	 */
	void SetLocalPosition(const Vector& position);

	/**
	 * @brief 设置本地缩放
	 * @param sx, sy 缩放系数
	 */
	void SetLocalScale(float sx, float sy);

	/**
	 * @brief 设置本地旋转角度
	 * @param rotation 角度 (度)
	 */
	void SetLocalRotation(float rotation);

	/**
	 * 水平镜像（仅渲染）：绕动画局部 x = pivotX 的竖直轴翻转。
	 * 注意 SetLocalScale/mLocalScaleX 是历史死字段（绘制不读），翻转必须走本接口。
	 * 不影响碰撞箱/影子/_ground 轨道速度/帧事件（魅惑僵尸的移动方向由 ZombieMove 按 mIsMindControlled 处理）。
	 */
	void SetFlipX(bool flip, float pivotX = 0.0f);

private:
	// 子动画相对于父轨道的本地变换
	float mLocalPosX = 0.0f;
	float mLocalPosY = 0.0f;
	float mLocalScaleX = 1.0f;
	float mLocalScaleY = 1.0f;
	float mLocalRotation = 0.0f;   // 角度制

	bool  mFlipX = false;
	float mFlipPivotX = 0.0f;

private:
	/**
	 * @brief 获取指定轨道的插值变换结果 (包含混合)
	 * @param trackIndex 轨道索引
	 * @return 插值后的帧变换
	 */
	TrackFrameTransform GetInterpolatedTransform(int trackIndex, float blendRatio) const;

	/**
	 * @brief 根据轨道名获取所有 TrackExtraInfo 指针
	 * @param trackName 轨道名
	 * @return 指针数组
	 */
	std::vector<TrackExtraInfo*> GetTrackExtrasByName(const std::string& trackName);

	/**
	 * @brief Draw 的内部递归实现：不 push/pop 根矩阵，纯计算并直接提交 g->DrawTextureMatrix
	 * @param g Graphics 对象
	 * @param baseX 基准 X
	 * @param baseY 基准 Y
	 * @param Scale 全局缩放
	 */
	void DrawInternal(Graphics* g, float baseX, float baseY, float Scale) const;

	/**
	 * @brief Task 5: fast path of DrawInternal for animators WITHOUT child sub-animators.
	 *        Builds one InstanceRecord per visible track (plus glow/overlay copies) and
	 *        emits via g->AppendReanimInstance — no per-call mat4 ctor, no DrawTextureMatrix.
	 *        Mat is pre-multiplied 2x3 with sprite (w*Scale, h*Scale) baked into tA..tD.
	 */
	void DrawInternalInstanced(Graphics* g, float baseX, float baseY, float Scale) const;
};

#endif