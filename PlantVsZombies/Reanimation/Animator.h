#pragma once
#ifndef _ANIMATOR_H
#define _ANIMATOR_H

#include "ReanimTypes.h"
#include "Reanimation.h"
#include "../Graphics.h"
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <map>
#include <memory>
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

/**
 * @brief 动画绘制命令结构体，用于缓存或直接绘制
 */
struct AnimDrawCommand {
    const GLTexture* texture;   ///< 纹理指针
    BlendMode blendMode;        ///< 混合模式
    glm::vec4 color;            ///< 颜色 (RGBA)
    float points[8];            ///< 4个顶点的世界坐标 (x1,y1, x2,y2, x3,y3, x4,y4)
};

class Animator {
private:
    std::shared_ptr<Reanimation> mReanim;   ///< 关联的 Reanimation 数据
    float mFPS = 12.0f;                      ///< 帧率
    float mFrameIndexNow = 0.0f;             ///< 当前帧索引 (浮点，支持插值)
    float mFrameIndexBegin = 0.0f;           ///< 起始帧索引
    float mFrameIndexEnd = 0.0f;              ///< 结束帧索引
    float mSpeed = 1.0f;                      ///< 播放速度倍率
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
    float mOriginalSpeed = 1.0f;                ///< 原始速度 (用于恢复)

    std::unordered_multimap<int, std::function<void()>> mFrameEvents;  ///< 帧事件表 (一次性)

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
     * @brief 添加帧事件 (一次性，触发后自动移除)
     * @param frameIndex 帧索引 (整数)
     * @param callback 回调函数
     */
    void AddFrameEvent(int frameIndex, std::function<void()> callback);

    /**
     * @brief 播放指定轨道动画，支持过渡效果
     * @param trackName 轨道名
     * @param speed 播放速度倍率 (默认1.0)
     * @param blendTime 过渡时间 (秒)，0表示无过渡
     * @return 是否成功
     */
    bool PlayTrack(const std::string& trackName, float speed = 1.0f, float blendTime = 0);

    /**
     * @brief 播放指定轨道动画一次，播放完后可切换回另一轨道
     * @param trackName 要播放的轨道名
     * @param returnTrack 播放完后要返回的轨道名 (为空则不切换)
     * @param speed 播放速度倍率
     * @param blendTime 过渡时间
     * @return 是否成功
     */
    bool PlayTrackOnce(const std::string& trackName,
        const std::string& returnTrack = "",
        float speed = 1.0f,
        float blendTime = 0);

    /**
     * @brief 设置原始速度 (用于恢复)
     */
    void SetOriginalSpeed(float speed) { this->mOriginalSpeed = speed; }

    /**
     * @brief 获取原始速度
     */
    float GetOriginalSpeed() { return this->mOriginalSpeed; }

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
    void SetTrackImage(const std::string& trackName, const GLTexture* image);

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
     * @brief 获取指定轨道的运动速度 (基于当前帧的前后位置差)
     * @param trackName 轨道名
     * @return 速度值 (像素/秒？实际为帧间位移乘以速度倍率)
     */
    float GetTrackVelocity(const std::string& trackName) const;

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
     * @brief 立即绘制动画 (若未调用 PrepareCommands，则实时计算)
     * @param g Graphics 对象
     * @param baseX 基准 X 坐标 (世界坐标)
     * @param baseY 基准 Y 坐标
     * @param Scale 全局缩放系数
     */
    void Draw(Graphics* g, float baseX, float baseY, float Scale = 1.0f);

    /**
     * @brief 预计算绘制命令并缓存 (可在任意线程调用，不涉及 OpenGL)
     * @param baseX 基准 X 坐标
     * @param baseY 基准 Y 坐标
     * @param Scale 全局缩放系数
     */
    void PrepareCommands(float baseX, float baseY, float Scale = 1.0f);

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

private:
    // 子动画相对于父轨道的本地变换
    float mLocalPosX = 0.0f;
    float mLocalPosY = 0.0f;
    float mLocalScaleX = 1.0f;
    float mLocalScaleY = 1.0f;
    float mLocalRotation = 0.0f;   // 角度制

    std::vector<AnimDrawCommand> mCachedCommands;  ///< PrepareCommands() 的缓存结果

private:
    /**
     * @brief 获取指定轨道的插值变换结果 (包含混合)
     * @param trackIndex 轨道索引
     * @return 插值后的帧变换
     */
    TrackFrameTransform GetInterpolatedTransform(int trackIndex) const;

    /**
     * @brief 根据轨道名获取所有 TrackExtraInfo 指针
     * @param trackName 轨道名
     * @return 指针数组
     */
    std::vector<TrackExtraInfo*> GetTrackExtrasByName(const std::string& trackName);

    /**
     * @brief 根据轨道名获取第一个匹配的轨道索引
     * @param trackName 轨道名
     * @return 索引，-1 表示未找到
     */
    int GetFirstTrackIndexByName(const std::string& trackName) const;

    /**
     * @brief 收集所有绘制命令 (递归调用子动画)
     * @param outCommands 输出命令列表
     * @param baseX 基准 X
     * @param baseY 基准 Y
     * @param Scale 全局缩放
     */
    void CollectDrawCommands(std::vector<AnimDrawCommand>& outCommands, float baseX, float baseY, float Scale) const;
};

#endif