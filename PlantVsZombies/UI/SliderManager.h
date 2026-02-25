#pragma once
#ifndef _SLIDERMANAGER_H
#define _SLIDERMANAGER_H
#include "../UI/Slider.h"
#include <vector>
#include <memory>

class SliderManager
{
private:
    std::vector<std::shared_ptr<Slider>> sliders; // 鏅鸿兘鎸囬拡绠＄悊婊戝姩鏉?

public:
    // 鍒涘缓婊戝姩鏉″苟杩斿洖鎸囬拡
    std::shared_ptr<Slider> CreateSlider(Vector pos = Vector::zero(),
        Vector size = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    // 澶勭悊鎵€鏈夋粦鍔ㄦ潯榧犳爣浜嬩欢
    void ProcessMouseEvent(InputHandler* input);

    // 鏇存柊鎵€鏈夋粦鍔ㄦ潯
    void UpdateAll(InputHandler* input);

    // 娓叉煋鎵€鏈夋粦鍔ㄦ潯
    void DrawAll(Graphics* g) const;

    // 娣诲姞鐜版湁婊戝姩鏉?
    void AddSlider(std::shared_ptr<Slider> slider);

    // 绉婚櫎婊戝姩鏉?
    void RemoveSlider(std::shared_ptr<Slider> slider);

    // 鍒犻櫎鍏ㄩ儴婊戝姩鏉?
    void ClearAllSliders();

    // 鑾峰彇婊戝姩鏉℃暟閲?
    size_t GetSliderCount() const;

    // 閫氳繃绱㈠紩鑾峰彇婊戝姩鏉?
    std::shared_ptr<Slider> GetSlider(size_t index) const;
};

#endif