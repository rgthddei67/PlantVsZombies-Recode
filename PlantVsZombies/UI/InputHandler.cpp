#include "InputHandler.h"
#include <iostream>
#include "../Graphics.h"

InputHandler::InputHandler(Graphics* graphics)
{
	mGraphics = graphics;
	// m_mouseButtons/m_prevMouseButtons/m_mousePosition/m_mouseDelta 由头文件就地初始化
}

void InputHandler::ProcessEvent(SDL_Event* event)
{
	switch (event->type)
	{
	case SDL_KEYDOWN:
	{
		SDL_Keycode key = event->key.keysym.sym;
		if (m_keyStates[key] == KeyState::UP) {
			m_keyStates[key] = KeyState::PRESSED;
		}
	}
	break;

	case SDL_KEYUP:
	{
		SDL_Keycode key = event->key.keysym.sym;
		if (m_keyStates[key] == KeyState::DOWN || m_keyStates[key] == KeyState::PRESSED) {
			m_keyStates[key] = KeyState::RELEASED;
		}
	}
	break;

	case SDL_MOUSEMOTION:
	{
		// 唯一的坐标咽喉点：SDL 给的是帧缓冲像素，全屏 letterbox 下先逆变换回逻辑坐标，
		// 再存入 m_mousePosition。这样下游所有消费者（Button/Slider 比逻辑坐标、
		// Scene/CardSlotManager 经 LogicalToWorld 转世界坐标）都无需改动，
		// 语义与窗口模式完全一致。窗口模式 scale=1/offset=0 时此换算是恒等。
		glm::vec2 logical = mGraphics->ScreenToLogical(
			static_cast<float>(event->motion.x),
			static_cast<float>(event->motion.y));
		m_mousePosition = Vector(logical.x, logical.y);
		break;
	}

	case SDL_MOUSEBUTTONDOWN:
		if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
			int index = event->button.button - 1;

			if (m_mouseButtons[index] == KeyState::UP) {
				m_mouseButtons[index] = KeyState::PRESSED;
			}
		}
		break;

	case SDL_MOUSEBUTTONUP:
		if (event->button.button >= SDL_BUTTON_LEFT && event->button.button <= SDL_BUTTON_X2) {
			int index = event->button.button - 1;

			if (m_mouseButtons[index] == KeyState::DOWN || m_mouseButtons[index] == KeyState::PRESSED) {
				m_mouseButtons[index] = KeyState::RELEASED;
			}
		}
		break;

	case SDL_MOUSEWHEEL:
		break;
	}
}

void InputHandler::Update()
{
	Vector prevMousePos = m_mousePosition;

	for (int i = 0; i < 5; i++)
	{
		m_prevMouseButtons[i] = m_mouseButtons[i];
	}
	m_prevKeyStates = m_keyStates;

	m_mouseDelta = m_mousePosition - prevMousePos;

	// PRESSED -> DOWN, RELEASED -> UP
	for (auto& pair : m_keyStates)
	{
		if (pair.second == KeyState::PRESSED) {
			pair.second = KeyState::DOWN;
		}
		else if (pair.second == KeyState::RELEASED) {
			pair.second = KeyState::UP;
		}
	}

	for (int i = 0; i < 5; i++)
	{
		if (m_mouseButtons[i] == KeyState::PRESSED) {
			m_mouseButtons[i] = KeyState::DOWN;
		}
		else if (m_mouseButtons[i] == KeyState::RELEASED) {
			m_mouseButtons[i] = KeyState::UP;
		}
	}
}

KeyState InputHandler::GetKeyState(SDL_Keycode keyCode) const
{
	auto it = m_keyStates.find(keyCode);
	if (it != m_keyStates.end())
	{
		return it->second;
	}
	return KeyState::UP;
}

bool InputHandler::IsKeyDown(SDL_Keycode keyCode) const
{
	KeyState state = GetKeyState(keyCode);
	return state == KeyState::DOWN || state == KeyState::PRESSED;
}

bool InputHandler::IsKeyPressed(SDL_Keycode keyCode) const
{
	return GetKeyState(keyCode) == KeyState::PRESSED;
}

bool InputHandler::IsKeyReleased(SDL_Keycode keyCode) const
{
	return GetKeyState(keyCode) == KeyState::RELEASED;
}

Vector InputHandler::GetMousePosition() const
{
	// m_mousePosition 已是逻辑坐标（letterbox 逆变换在 ProcessEvent 入口完成）。
	return m_mousePosition;
}

Vector InputHandler::GetMouseWorldPosition() const {
	Vector mousePositon = GetMousePosition();
	// 入参已是逻辑坐标，LogicalToWorld 在此基础上做相机逆变换。
	return mGraphics->LogicalToWorld(mousePositon.x, mousePositon.y);
}

Vector InputHandler::GetMouseDelta() const
{
	return m_mouseDelta;
}

KeyState InputHandler::GetMouseButtonState(Uint8 button) const
{
	if (button >= SDL_BUTTON_LEFT && button <= SDL_BUTTON_X2)
	{
		int index = button - 1;
		return m_mouseButtons[index];
	}
	return KeyState::UP;
}

bool InputHandler::IsMouseButtonDown(Uint8 button) const
{
	KeyState state = GetMouseButtonState(button);
	return state == KeyState::DOWN || state == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonPressed(Uint8 button) const
{
	return GetMouseButtonState(button) == KeyState::PRESSED;
}

bool InputHandler::IsMouseButtonReleased(Uint8 button) const
{
	return GetMouseButtonState(button) == KeyState::RELEASED;
}