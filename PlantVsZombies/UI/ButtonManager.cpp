#include "ButtonManager.h"

std::shared_ptr<Button> ButtonManager::CreateButton(Vector pos, Vector size)
{
    auto button = std::make_shared<Button>(pos, size);
    buttons.push_back(button);
    return button;
}

void ButtonManager::AddButton(std::shared_ptr<Button> button)
{
    buttons.push_back(button);
}

void ButtonManager::RemoveButton(std::shared_ptr<Button> button)
{
    for (auto it = buttons.begin(); it != buttons.end(); ++it)
    {
        if (*it == button)
        {
            buttons.erase(it);
            break;
        }
    }
}

void ButtonManager::ClearAllButtons()
{
    buttons.clear();
}

void ButtonManager::UpdateAll(InputHandler* input)
{
    if (!input) return;
    for (size_t i = 0; i < buttons.size(); i++)
    {
        if (buttons[i])
        {
            buttons[i]->Update(input);
        }
    }
}

void ButtonManager::DrawAll(Graphics* g) const
{
    if (!g) return;

    for (size_t i = 0; i < buttons.size(); i++)
    {
        if (buttons[i] && !buttons[i]->IsSkipDraw())
        {
            buttons[i]->Draw(g);
        }
    }
}

size_t ButtonManager::GetButtonCount() const
{
    return buttons.size();
}

std::shared_ptr<Button> ButtonManager::GetButton(size_t index) const
{
    if (index < buttons.size())
    {
        return buttons[index];
    }
    return nullptr;
}

void ButtonManager::ProcessMouseEvent(InputHandler* input)
{
    for (size_t i = 0; i < buttons.size(); i++)
    {
        if (buttons[i])
        {
            buttons[i]->ProcessMouseEvent(input);
        }
    }
}

void ButtonManager::ResetAllFrameStates()
{
    for (size_t i = 0; i < buttons.size(); i++)
    {
        if (buttons[i])
        {
            buttons[i]->ResetFrameState();
        }
    }
}