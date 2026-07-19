#pragma once
#ifndef _H_MESSAGEBOX_H
#define _H_MESSAGEBOX_H

#include "../Game/GameObject.h"
#include "../Graphics.h"
#include "Button.h"
#include "Slider.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class GameMessageBox : public GameObject {
public:
	class Builder;

	struct ButtonConfig {
		std::string text;
		Vector pos;
		Vector size;   // 大小，如果是Vector::zero就是按照NormalButton处理
		float fontsize;
		std::function<void()> callback;
		std::string texture;
		bool autoClose = true;             // 是否自动关闭
		bool enabled = true;               // false 时保留按钮外观，但不参与命中与点击
		bool initChecked = false;          // 仅 checkbox 有效：创建时的初始勾选态
	};

	struct SliderConfig {
		Vector pos;
		Vector size;
		float min;
		float max;
		float initValue;    // 初始化的值
		std::function<void(float)> callback;
		bool integerOnly = false;   // true=滑块只能停在整数刻度上（省略则为普通连续滑块）
	};

	struct TextConfig {
		Vector pos;
		float size;
		std::string text;
		glm::vec4 color;
		std::string font = ResourceKeys::Fonts::FONT_FZCQ;
	};

	GameMessageBox(const Vector& pos,
		const std::string& message,
		const std::vector<ButtonConfig>& buttons,
		const std::vector<SliderConfig>& sliders,
		const std::vector<TextConfig>& texts,
		const std::string& title = "",
		const std::string& backgroundImageKey = "",
		float scale = 1.0f,
		const Vector& explicitSize = Vector(0.0f, 0.0f));   // 非零=用此尺寸并以 pos 居中绘制背景（自动决定大小）

	~GameMessageBox();

	virtual void Start() override;
	virtual void Draw(Graphics* g) override;

	void Close();

private:
	Vector m_position;
	float m_scale = 1.0f;
	Vector m_size;
	Vector m_explicitSize{ 0.0f, 0.0f };   // 非零时覆盖纹理尺寸，背景以 m_position 居中绘制
	std::string m_title;
	std::string m_message;
	std::string m_backgroundImageKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
	std::vector<ButtonConfig> m_buttonConfigs;
	std::vector<SliderConfig> m_sliderConfigs;
	std::vector<TextConfig> m_textConfigs;

	std::vector<std::shared_ptr<Button>> m_buttons;
	std::vector<std::shared_ptr<Slider>> m_sliders;

	glm::vec4 m_textColor = { 245, 214, 127, 255 };
	glm::vec4 m_titleColor = { 53, 191, 61, 255 };

	Vector GetBackgroundOriginalSize() const;
};

// 流式构建器：把 9 参构造与隐式规则（空key+explicitSize=纯色面板、CHECKBOX纹理嗅探）
// 显式化为命名方法。终结方法 Show() 创建对象并返回 shared_ptr。
class GameMessageBox::Builder {
public:
	explicit Builder(const Vector& pos) : m_pos(pos) {}

	// —— 背景（不调用 = 默认 IMAGE_MESSAGEBOX 纹理）；后调覆盖先调 ——
	Builder& Panel(float w, float h) {                    // 纯色面板，尺寸 w×h，以 pos 居中
		m_bgKey.clear(); m_explicitSize = Vector(w, h); return *this;
	}
	Builder& Background(const std::string& key) {         // 纹理，原始尺寸×scale
		m_bgKey = key; m_explicitSize = Vector(0.0f, 0.0f); return *this;
	}
	Builder& Background(const std::string& key, const Vector& size) {  // 纹理+显式尺寸居中
		m_bgKey = key; m_explicitSize = size; return *this;
	}

	Builder& Title(const std::string& t)   { m_title = t;   return *this; }
	Builder& Message(const std::string& m) { m_message = m; return *this; }

	Builder& Text(const Vector& pos, float fontSize, const std::string& text,
		const glm::vec4& color, const std::string& font = ResourceKeys::Fonts::FONT_FZCQ) {
		m_texts.push_back({ pos, fontSize, text, color, font });
		return *this;
	}

	Builder& Button(const std::string& text, const Vector& pos, const Vector& size,
		float fontSize, std::function<void()> cb,
		const std::string& texture = ResourceKeys::Textures::IMAGE_BUTTONSMALL,
		bool autoClose = true, bool enabled = true) {
		m_buttons.push_back({ text, pos, size, fontSize, std::move(cb), texture, autoClose, enabled, false });
		return *this;
	}

	Builder& Checkbox(const Vector& pos, const Vector& size,
		std::function<void()> cb, bool initChecked = false) {
		m_buttons.push_back({ "", pos, size, 1.0f, std::move(cb),
			ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0, false, true, initChecked });
		return *this;
	}

	Builder& Slider(const Vector& pos, const Vector& size, float minVal, float maxVal,
		float initValue, std::function<void(float)> cb, bool integerOnly = false) {
		m_sliders.push_back({ pos, size, minVal, maxVal, initValue, std::move(cb), integerOnly });
		return *this;
	}

	Builder& Scale(float s) { m_scale = s; return *this; }

	std::shared_ptr<GameMessageBox> Show();   // 实现在 .cpp（依赖 GameObjectManager）

private:
	Vector m_pos;
	std::string m_title;
	std::string m_message;
	std::string m_bgKey = ResourceKeys::Textures::IMAGE_MESSAGEBOX;
	float m_scale = 1.0f;
	Vector m_explicitSize{ 0.0f, 0.0f };
	std::vector<GameMessageBox::ButtonConfig> m_buttons;
	std::vector<GameMessageBox::SliderConfig> m_sliders;
	std::vector<GameMessageBox::TextConfig>   m_texts;
};

#endif
