#pragma once
#include <string>
#include <SDL2/SDL.h>
#include <vector>
#include "Definit.h"

struct FlagMarker {
    SDL_Texture* texture1;   // 第一个图片纹理（如旗杆）
    SDL_Texture* texture2;   // 第二个图片纹理（如旗面）
    float position;          // 0~1 从左到右

    // 升起动画
    float currentRaiseY = 0.0f;         // 当前Y偏移（像素，向下为正）
    float targetRaiseY = 0.0f;          // 目标Y偏移
    float raiseSpeed = 1.0f;            // 移动速度（像素/秒）
};

class FlagMeter
{
public:
    FlagMeter(Vector position, float initialProgress = 0.0f);
    ~FlagMeter() = default;

    void SetProgress(float progress);              // 0.0 ~ 1.0
    float GetProgress() const { return m_progress; }

    void SetPosition(Vector pos) { m_position = pos; }
    Vector GetPosition() const { return m_position; }

    // 设置4张图片的纹理
    void SetImages(SDL_Texture* bgTex, SDL_Texture* fillTex, SDL_Texture* headTex, SDL_Texture* middleTex);

    // 绘制函数
    void Draw(SDL_Renderer* renderer) const;
    void Update(float deltaTime);

    void AddFlag(SDL_Texture* tex1, SDL_Texture* tex2, float position);
    void ClearFlags();
    void SetFlags(const std::vector<FlagMarker>& flags);
    size_t GetFlagCount() const { return m_flags.size(); }

    // 设置显示范围的左右边界比例（0~1）
    void SetBounds(float leftBound, float rightBound);

    // 升起指定索引的旗子（索引从0开始）
    void RaiseFlag(int index, float targetY, float duration);
    // 直接设置升起状态（无动画）
    void SetFlagRaiseImmediate(int index, float targetY);

private:
    Vector m_position;          // 背景左上角坐标
    float m_progress;            // 0~1

    SDL_Texture* m_bgTexture = nullptr;      // 背景纹理
    SDL_Texture* m_fillTexture = nullptr;     // 填充条纹理
    SDL_Texture* m_headTexture = nullptr;     // 头部小旗纹理
    SDL_Texture* m_middleTexture = nullptr;   // 中间装饰纹理

    std::vector<FlagMarker> m_flags;

    float m_leftBound = 0.11f;
    float m_rightBound = 1.0f;

    // 获取纹理尺寸
    SDL_Rect GetTextureRect(SDL_Texture* tex) const;
};