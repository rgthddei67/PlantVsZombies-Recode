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
    std::vector<std::shared_ptr<Button>> buttons; 

public:
    std::shared_ptr<Button> CreateButton(Vector pos = Vector::zero(),
        Vector size = Vector(40, 40));

    void ProcessMouseEvent(InputHandler* input);

    void ResetAllFrameStates();

    void AddButton(std::shared_ptr<Button> button);

    void RemoveButton(std::shared_ptr<Button> button);

    void ClearAllButtons();

    void UpdateAll(InputHandler* input);

    void DrawAll(Graphics* g) const;

    size_t GetButtonCount() const;

    std::shared_ptr<Button> GetButton(size_t index) const;
};

#endif