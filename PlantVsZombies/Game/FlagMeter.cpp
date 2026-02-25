#include "FlagMeter.h"
#include "../ResourceManager.h"
#include <algorithm>
#include <cmath>

FlagMeter::FlagMeter(Vector position, float initialProgress)
    : m_position(position), m_progress(std::clamp(initialProgress, 0.0f, 1.0f))
{
    m_flags.reserve(16);
}

void FlagMeter::SetProgress(float progress)
{
    m_progress = std::clamp(progress, 0.0f, 1.0f);
}

void FlagMeter::SetImages(const GLTexture* bgTex, const GLTexture* fillTex,
    const GLTexture* headTex, const GLTexture* middleTex)
{
    m_bgTexture = bgTex;
    m_fillTexture = fillTex;
    m_headTexture = headTex;
    m_middleTexture = middleTex;
}

void FlagMeter::GetTextureSize(const GLTexture* tex, int& w, int& h) const
{
    if (tex) {
        w = tex->width;
        h = tex->height;
    }
    else {
        w = h = 0;
    }
}

void FlagMeter::SetBounds(float leftBound, float rightBound)
{
    m_leftBound = std::clamp(leftBound, 0.0f, 1.0f);
    m_rightBound = std::clamp(rightBound, m_leftBound, 1.0f);
}

void FlagMeter::Draw(Graphics* g) const
{
    if (!g) return;

    // 获取背景纹理尺寸
    int bgW = 0, bgH = 0;
    GetTextureSize(m_bgTexture, bgW, bgH);
    if (bgW <= 0 || bgH <= 0) return;  // 无背景则不绘制

    // 计算左右边界的屏幕像素坐标
    float leftBoundPx = m_position.x + bgW * m_leftBound;
    float rightBoundPx = m_position.x + bgW * m_rightBound;
    float usableWidth = rightBoundPx - leftBoundPx;

    // 将进度映射到边界内（用于头部和填充条）
    float mappedProgress = m_leftBound + m_progress * (m_rightBound - m_leftBound);
    float headCenterX = m_position.x + bgW * mappedProgress;
    headCenterX = std::clamp(headCenterX, leftBoundPx, rightBoundPx);

    // 1. 绘制背景图
    if (m_bgTexture) {
        g->DrawTexture(m_bgTexture, m_position.x, m_position.y,
            static_cast<float>(bgW), static_cast<float>(bgH));
    }

    // 2. 绘制填充条（根据进度裁剪，并受边界限制）
    if (m_fillTexture && m_progress >= 0.0f) {
        int fillW = 0, fillH = 0;
        GetTextureSize(m_fillTexture, fillW, fillH);
        if (fillW > 0 && fillH > 0) {
            float fillRatio = 1.0f - m_progress;   // 已完成比例
            // 源矩形：从右侧裁剪
            float srcX = fillW * (1.0f - fillRatio);  // 裁剪掉左侧未完成部分
            float srcY = 0.0f;
            float srcW = fillW * fillRatio;
            float srcH = static_cast<float>(fillH);

            // 目标矩形：左边缘与头部对齐，宽度基于可用宽度
            float fillLeftX = headCenterX;
            float fillBarWidth = usableWidth * fillRatio;
            float fillRightX = fillLeftX + fillBarWidth;

            // 钳位右边缘不超出右边界
            if (fillRightX > rightBoundPx)
                fillRightX = rightBoundPx;
            // 钳位左边缘不超出左边界
            if (fillLeftX < leftBoundPx)
                fillLeftX = leftBoundPx;

            float actualWidth = fillRightX - fillLeftX;
            if (actualWidth > 0) {
                // 使用 Graphics 的带源矩形绘制方法
                g->DrawTextureRegion(m_fillTexture,
                    srcX, srcY, srcW, srcH,
                    fillLeftX, m_position.y,
                    actualWidth, static_cast<float>(bgH));
            }
        }
    }

    // 3. 绘制进度条上的旗子
    for (const auto& flag : m_flags) {
        // 旗子基准点水平位置已映射到边界内
        float flagBaseX = m_position.x + bgW * (m_leftBound + flag.position * (m_rightBound - m_leftBound));
        flagBaseX = std::clamp(flagBaseX, leftBoundPx, rightBoundPx);
        float flagBaseY = m_position.y + bgH / 2.0f - 4;  // 垂直中心基准点

        // 绘制第一个图片（旗杆）
        if (flag.texture1) {
            int w = 0, h = 0;
            GetTextureSize(flag.texture1, w, h);
            float drawX = flagBaseX - w / 2.0f;
            float drawY = flagBaseY - h / 2.0f;
            g->DrawTexture(flag.texture1, drawX, drawY,
                static_cast<float>(w), static_cast<float>(h));
        }

        // 绘制第二个图片（旗面，应用升起偏移）
        if (flag.texture2) {
            int w = 0, h = 0;
            GetTextureSize(flag.texture2, w, h);
            float drawX = flagBaseX - w / 2.0f;
            float drawY = flagBaseY - h / 2.0f + flag.currentRaiseY;
            g->DrawTexture(flag.texture2, drawX, drawY,
                static_cast<float>(w), static_cast<float>(h));
        }
    }

    // 4. 绘制中间装饰图片（固定在背景中央）
    if (m_middleTexture) {
        int w = 0, h = 0;
        GetTextureSize(m_middleTexture, w, h);
        float drawX = m_position.x + (bgW - w) / 2.0f;
        float drawY = m_position.y + (bgH - h) / 2.0f + 4;
        g->DrawTexture(m_middleTexture, drawX, drawY,
            static_cast<float>(w), static_cast<float>(h));
    }

    // 5. 绘制头部
    if (m_headTexture) {
        int w = 0, h = 0;
        GetTextureSize(m_headTexture, w, h);
        float headY = m_position.y + bgH / 2.0f - h / 2.0f - 4;
        float drawX = headCenterX - w / 2.0f;
        g->DrawTexture(m_headTexture, drawX, headY,
            static_cast<float>(w), static_cast<float>(h));
    }
}

void FlagMeter::Update(float deltaTime)
{
    for (auto& flag : m_flags) {
        if (flag.currentRaiseY != flag.targetRaiseY) {
            float diff = flag.targetRaiseY - flag.currentRaiseY;
            float step = flag.raiseSpeed * deltaTime;
            if (std::abs(diff) <= step) {
                flag.currentRaiseY = flag.targetRaiseY;
            }
            else {
                flag.currentRaiseY += (diff > 0 ? step : -step);
            }
        }
    }
}

void FlagMeter::AddFlag(const GLTexture* tex1, const GLTexture* tex2, float position)
{
    m_flags.push_back({ tex1, tex2, std::clamp(position, 0.0f, 1.0f) });
}

void FlagMeter::ClearFlags()
{
    m_flags.clear();
}

void FlagMeter::SetFlags(const std::vector<FlagMarker>& flags)
{
    m_flags = flags;
    for (auto& flag : m_flags) {
        flag.position = std::clamp(flag.position, 0.0f, 1.0f);
        flag.currentRaiseY = 0.0f;
        flag.targetRaiseY = 0.0f;
        flag.raiseSpeed = 0.0f;
    }
}

void FlagMeter::RaiseFlag(int index, float targetY, float duration)
{
    if (index >= 0 && index < static_cast<int>(m_flags.size())) {
        auto& flag = m_flags[index];
        flag.targetRaiseY = targetY;
        if (duration > 0.0f) {
            float diff = targetY - flag.currentRaiseY;
            flag.raiseSpeed = std::abs(diff) / duration;
        }
        else {
            flag.currentRaiseY = targetY;
            flag.raiseSpeed = 0.0f;
        }
    }
}

void FlagMeter::SetFlagRaiseImmediate(int index, float targetY)
{
    if (index >= 0 && index < static_cast<int>(m_flags.size())) {
        m_flags[index].currentRaiseY = targetY;
        m_flags[index].targetRaiseY = targetY;
        m_flags[index].raiseSpeed = 0.0f;
    }
}