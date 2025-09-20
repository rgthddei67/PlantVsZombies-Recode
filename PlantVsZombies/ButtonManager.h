#pragma once
#ifndef _BUTTONMANAGER_H
#define _BUTTONMANAGER_H

#include "Button.h"
#include <vector>
#include <memory>

class ButtonManager
{
private:
    std::vector<std::shared_ptr<Button>> buttons; // ����ָ�����ť

public:
    // ������ť������ָ��
    std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(),
        Vector size = Vector(40, 40));

    // �������а�ť����ƶ��¼�
    void ProcessMouseEvent(SDL_Event* event);

    // ������а�ť����¼
    void ResetAllFrameStates();

    // ������а�ť
    void AddButton(std::shared_ptr<Button> button);

    // �Ƴ���ť
    void RemoveButton(std::shared_ptr<Button> button);

    // ɾ��ȫ����ť
    void ClearAllButtons();

    // ͳһ�������а�ť
    void UpdateAll(InputHandler* input);

    // ͳһ��Ⱦ���а�ť
    void DrawAll(SDL_Renderer* renderer, const std::vector<SDL_Texture*>& textures) const;

    // ��ȡ��ť����
    size_t GetButtonCount() const;

    // ͨ��������ȡ��ť
    std::shared_ptr<Button> GetButton(size_t index) const;
};

#endif