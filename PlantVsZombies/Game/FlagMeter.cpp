#include "FlagMeter.h"
#include "../ResourceManager.h"
#include <algorithm>

FlagMeter::FlagMeter(Vector position, float initialProgress)
    : m_position(position), m_progress(std::clamp(initialProgress, 0.0f, 1.0f))
{
    m_flags.reserve(16);
}

void FlagMeter::SetProgress(float progress)
{
    m_progress = std::clamp(progress, 0.0f, 1.0f);
}

void FlagMeter::SetImages(SDL_Texture* bgTex, SDL_Texture* fillTex, SDL_Texture* headTex, SDL_Texture* middleTex)
{
    m_bgTexture = bgTex;
    m_fillTexture = fillTex;
    m_headTexture = headTex;
    m_middleTexture = middleTex;
}

SDL_Rect FlagMeter::GetTextureRect(SDL_Texture* tex) const
{
    SDL_Rect rect = { 0, 0, 0, 0 };
    if (tex)
        SDL_QueryTexture(tex, nullptr, nullptr, &rect.w, &rect.h);
    return rect;
}

void FlagMeter::SetBounds(float leftBound, float rightBound)
{
    m_leftBound = std::clamp(leftBound, 0.0f, 1.0f);
    m_rightBound = std::clamp(rightBound, m_leftBound, 1.0f);
}

void FlagMeter::Draw(SDL_Renderer* renderer) const
{
    // 获取背景纹理尺寸
    SDL_Rect bgRect = GetTextureRect(m_bgTexture);
    int bgWidth = bgRect.w > 0 ? bgRect.w : 200;
    int bgHeight = bgRect.h > 0 ? bgRect.h : 30;

    // 计算左右边界的屏幕像素坐标
    float leftBoundPx = m_position.x + bgWidth * m_leftBound;
    float rightBoundPx = m_position.x + bgWidth * m_rightBound;
    float usableWidth = rightBoundPx - leftBoundPx;

    // 将进度映射到边界内（用于头部和填充条）
    float mappedProgress = m_leftBound + m_progress * (m_rightBound - m_leftBound);
    float headCenterX = m_position.x + bgWidth * mappedProgress;
    // 钳位头部中心，确保不超出边界
    headCenterX = std::clamp(headCenterX, leftBoundPx, rightBoundPx);

    // 1. 绘制背景图
    if (m_bgTexture)
    {
        SDL_Rect destBg = bgRect;
        destBg.x = static_cast<int>(m_position.x);
        destBg.y = static_cast<int>(m_position.y);
        SDL_RenderCopy(renderer, m_bgTexture, nullptr, &destBg);
    }

    // 2. 绘制填充条（根据进度裁剪，并受边界限制）
    if (m_fillTexture && m_progress >= 0.0f)
    {
        int fillWidth, fillHeight;
        SDL_QueryTexture(m_fillTexture, nullptr, nullptr, &fillWidth, &fillHeight);

        float fillRatio = 1.0f - m_progress;   // 已完成比例
        int drawWidth = static_cast<int>(fillWidth * fillRatio);
        // 源矩形从右侧裁剪（因为填充条表示已完成部分，从头部向左延伸）
        SDL_Rect srcRect = { fillWidth - drawWidth, 0, drawWidth, fillHeight };

        // 填充条左边缘与头部对齐（但头部可能被钳位）
        float fillLeftX = headCenterX;
        // 填充条宽度基于可用宽度
        float fillBarWidth = usableWidth * fillRatio;
        float fillRightX = fillLeftX + fillBarWidth;

        // 钳位右边缘不超出右边界
        if (fillRightX > rightBoundPx)
            fillRightX = rightBoundPx;
        // 钳位左边缘不超出左边界（理论上不会小于 leftBoundPx，但安全起见）
        if (fillLeftX < leftBoundPx)
            fillLeftX = leftBoundPx;

        float actualWidth = fillRightX - fillLeftX;
        if (actualWidth > 0)
        {
            SDL_Rect destRect = {
                static_cast<int>(fillLeftX),
                static_cast<int>(m_position.y),
                static_cast<int>(actualWidth),
                bgHeight
            };
            SDL_RenderCopy(renderer, m_fillTexture, &srcRect, &destRect);
        }
    }

    // 3. 绘制进度条上的旗子
    for (const auto& flag : m_flags)
    {
        // 旗子基准点水平位置已映射到边界内
        float flagBaseX = m_position.x + bgWidth * (m_leftBound + flag.position * (m_rightBound - m_leftBound));
        // 钳位基准点，避免图片超出边界
        flagBaseX = std::clamp(flagBaseX, leftBoundPx, rightBoundPx);
        float flagBaseY = m_position.y + bgHeight / 2.0f - 4;  // 垂直中心基准点

        // 绘制第一个图片
        if (flag.texture1)
        {
            int w, h;
            SDL_QueryTexture(flag.texture1, nullptr, nullptr, &w, &h);
            SDL_Rect destRect = {
                static_cast<int>(flagBaseX - w / 2),
                static_cast<int>(flagBaseY - h / 2),
                w, h
            };
            SDL_RenderCopy(renderer, flag.texture1, nullptr, &destRect);
        }

        // 绘制第二个图片（应用升起偏移）
        if (flag.texture2)
        {
            int w, h;
            SDL_QueryTexture(flag.texture2, nullptr, nullptr, &w, &h);
            SDL_Rect destRect = {
                static_cast<int>(flagBaseX - w / 2),
                static_cast<int>(flagBaseY - h / 2 + flag.currentRaiseY),
                w, h
            };
            SDL_RenderCopy(renderer, flag.texture2, nullptr, &destRect);
        }
    }

    // 4. 绘制中间装饰图片（固定在背景中央）
    if (m_middleTexture)
    {
        int w, h;
        SDL_QueryTexture(m_middleTexture, nullptr, nullptr, &w, &h);

        SDL_Rect destRect = {
            static_cast<int>(m_position.x + (bgWidth - w) / 2),
            static_cast<int>(m_position.y + (bgHeight - h) / 2 + 4),
            w, h
        };
        SDL_RenderCopy(renderer, m_middleTexture, nullptr, &destRect);
    }

    // 5. 绘制头部
    if (m_headTexture)
    {
        int headWidth, headHeight;
        SDL_QueryTexture(m_headTexture, nullptr, nullptr, &headWidth, &headHeight);

        float headY = m_position.y + bgHeight / 2.0f - headHeight / 2.0f - 4;
        SDL_Rect headRect = {
            static_cast<int>(headCenterX - headWidth / 2),
            static_cast<int>(headY),
            headWidth, headHeight
        };
        SDL_RenderCopy(renderer, m_headTexture, nullptr, &headRect);
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

void FlagMeter::AddFlag(SDL_Texture* tex1, SDL_Texture* tex2, float position)
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