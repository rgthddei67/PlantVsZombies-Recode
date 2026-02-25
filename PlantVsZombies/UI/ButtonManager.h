#pragma once
#ifndef _BUTTONMANAGER_H
#define _BUTTONMANAGER_H
#include "../UI/Button.h"
#include "../UI/InputHandler.h"
#include <vector>
#include <memory>
#include "../Graphics.h"

class ButtonManager
{
private:
    std::vector<std::shared_ptr<Button>> buttons; // 鏅鸿兘鎸囬拡绠＄悊鎸夐挳

public:
    // 鍒涘缓鎸夐挳骞惰繑鍥炴寚閽?
    std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(),
        Vector size = Vector(40, 40));

    // 绠＄悊鎵€鏈夋寜閽紶鏍囩Щ鍔ㄤ簨浠?
    void ProcessMouseEvent(InputHandler* input);

    // 娓呯┖鎵€鏈夋寜閽紶鏍囪褰?
    void ResetAllFrameStates();

    // 娣诲姞鐜版湁鎸夐挳
    void AddButton(std::shared_ptr<Button> button);

    // 绉婚櫎鎸夐挳
    void RemoveButton(std::shared_ptr<Button> button);

    // 鍒犻櫎鍏ㄩ儴鎸夐挳
    void ClearAllButtons();

    // 缁熶竴鏇存柊鎵€鏈夋寜閽?
    void UpdateAll(InputHandler* input);

    // 缁熶竴娓叉煋鎵€鏈夋寜閽?
    void DrawAll(Graphics* g) const;

    // 鑾峰彇鎸夐挳鏁伴噺
    size_t GetButtonCount() const;

    // 閫氳繃绱㈠紩鑾峰彇鎸夐挳
    std::shared_ptr<Button> GetButton(size_t index) const;
};

#endif