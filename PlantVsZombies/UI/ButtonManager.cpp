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
    for (auto& button : buttons)
    {
        button->Update(input);
    }
}

void ButtonManager::DrawAll(SDL_Renderer* renderer) const
{
    for (const auto& button : buttons)
    {
        button->Draw(renderer);
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

void ButtonManager::ProcessMouseEvent(SDL_Event* event)
{
    for (auto& button : buttons) 
    {
        button->ProcessMouseEvent(event);
    }
}

void ButtonManager::ResetAllFrameStates()
{
    for (auto& button : buttons) 
    {
        button->ResetFrameState();
    }
}