#pragma once
#ifndef _SLIDER_H
#define _SLIDER_H
#include "../Game/Definit.h"
#include "../UI/InputHandler.h"
#include "../ResourceKeys.h"
#include <functional>
#include <string>
#include <SDL2/SDL.h>
#include "../Graphics.h"

class ResourceManager;

class Slider
{
private:
    Vector position = Vector::zero();          // 婊戝姩鏉′綅缃紙搴曢儴鑳屾櫙鐨勪綅缃級
    Vector size = Vector(135, 10);             // 婊戝姩鏉″ぇ灏?
    float minValue = 0.0f;                     // 鏈€灏忓€?
    float maxValue = 1.0f;                     // 鏈€澶у€?
    float currentValue = 0.5f;                 // 褰撳墠鍊?

    std::string backgroundImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_SLIDERSLOT;            // 鑳屾櫙鍥剧墖key
    std::string knobImageKey = ResourceKeys::Textures::IMAGE_OPTIONS_SLIDERKNOB2;                  // 婊戝潡鍥剧墖key

    bool isDragging = false;                   // 鏄惁姝ｅ湪鎷栧姩
	bool canDrag = true;                       // 鏄惁鍏佽鎷栧姩
    Vector dragStartPosition;                  // 鎷栧姩寮€濮嬩綅缃?
    float dragStartValue;                      // 鎷栧姩寮€濮嬫椂鐨勫€?

    int SliderSizeX = 22;
    int SliderSizeY = 29;

    std::function<void(float)> onChangeCallback = nullptr; // 鍊兼敼鍙樻椂鍊欑殑鍥炶皟

public:
    Slider(Vector createPosition = Vector::zero(),
        Vector sliderSize = Vector(135, 10),
        float minVal = 0.0f,
        float maxVal = 1.0f,
        float initialValue = 0.5f);

    void SetPosition(Vector pos);
    void SetSize(Vector size);
    void SetValueRange(float min, float max);
    void SetValue(float value);
    // 鑳藉惁鎷栧姩
	void SetDrag(bool canDrag);

    // 璁剧疆鍥剧墖璧勬簮key
    void SetImageKeys(const std::string& background, const std::string& knob);

    // 璁剧疆鍊兼敼鍙樺洖璋?
    void SetChangeCallBack(std::function<void(float)> callback);

    // 澶勭悊杈撳叆浜嬩欢
    void ProcessMouseEvent(InputHandler* input);
    void Update(InputHandler* input);

    // 娓叉煋
    void Draw(Graphics* g) const;

    // 鑾峰彇鐘舵€佸拰鍊?
    bool IsDragging() const;
    float GetValue() const;
    float GetNormalizedValue() const; // 鑾峰彇褰掍竴鍖栧€?(0-1)

    // 妫€娴嬬偣鏄惁鍦ㄦ粦鍧楀唴
    bool KnobContainsPoint(Vector point) const;

    // 妫€娴嬬偣鏄惁鍦ㄦ粦鍔ㄦ潯鑳屾櫙鍐?
    bool BackgroundContainsPoint(Vector point) const;

    // 鏍规嵁X浣嶇疆璁＄畻鍊?
    float CalculateValueFromX(float x) const;

    // 鏍规嵁鍊艰绠楁粦鍧梄浣嶇疆
    float CalculateKnobXFromValue() const;
};
#endif