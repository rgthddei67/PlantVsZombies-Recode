#pragma once
#ifndef _H_MESSAGEBOX_H
#define _H_MESSAGEBOX_H

#include "../Graphics.h"
#include "../ResourceKeys.h"
#include "Button.h"
#include "Slider.h"
#include <SDL2/SDL.h>
#include <string>
#include <functional>
#include <memory>
#include <vector>

class UIManager;

// 场景级模态面板。生命周期与控件注册均由创建它的 UIManager 管理，
// 不参与玩法对象的 GameObject 调度。
class GameMessageBox {
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
		std::string tooltipText;           // 非空时，悬停该按钮显示说明
		Vector hitSize{ 0.0f, 0.0f };      // 非零时，以 pos 为左上角扩展独立命中区域
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

	struct TooltipPanelConfig {
		Vector maxSize{ 0.0f, 0.0f };
		float fontSize = 17.0f;
		glm::vec4 textColor{ 245, 214, 127, 255 };
	};

	GameMessageBox(UIManager* owner,
		const Vector& pos,
		const std::string& message,
		const std::vector<ButtonConfig>& buttons,
		const std::vector<SliderConfig>& sliders,
		const std::vector<TextConfig>& texts,
		const std::string& title,
		const std::string& backgroundImageKey,
		float scale,
		const Vector& explicitSize,
		const TooltipPanelConfig& tooltipPanel);   // 非零 explicitSize=用此尺寸并以 pos 居中绘制背景

	~GameMessageBox();

	void Draw(Graphics* g);

	void SetActive(bool active);
	bool IsActive() const { return m_active; }
	void Close();
	/** 返回当前悬停控件的说明；没有可见说明时返回空字符串。 */
	const std::string& GetHoveredTooltipText() const;
	/** 返回随鼠标并经屏幕边缘修正后的说明框左上角；无说明时返回零向量。 */
	Vector GetTooltipDrawPosition() const;

private:
	friend class UIManager;

	UIManager* m_owner = nullptr;   // 非拥有指针；UIManager::ClearAll 会在销毁前解除关联
	bool m_active = true;
	bool m_closeRequested = false;
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
	TooltipPanelConfig m_tooltipPanel;

	std::vector<std::shared_ptr<Button>> m_buttons;
	std::vector<std::shared_ptr<Slider>> m_sliders;

	glm::vec4 m_textColor = { 245, 214, 127, 255 };
	glm::vec4 m_titleColor = { 53, 191, 61, 255 };

	void InitializeControls();
	void DetachControls();
	bool IsCloseRequested() const { return m_closeRequested; }
	Vector GetBackgroundOriginalSize() const;
	Vector GetTooltipDrawSize(const std::string& text) const;
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
		std::function<void()> cb, bool initChecked = false,
		const std::string& tooltipText = "", const Vector& hitSize = Vector(0.0f, 0.0f)) {
		m_buttons.push_back({ "", pos, size, 1.0f, std::move(cb),
			ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0, false, true, initChecked,
			tooltipText, hitSize });
		return *this;
	}

	/** 配置仅在带说明的控件被悬停时绘制的顶层浮动说明框。 */
	Builder& TooltipPanel(const Vector& maxSize, float fontSize,
		const glm::vec4& textColor = glm::vec4(245, 214, 127, 255)) {
		m_tooltipPanel = { maxSize, fontSize, textColor };
		return *this;
	}

	Builder& Slider(const Vector& pos, const Vector& size, float minVal, float maxVal,
		float initValue, std::function<void(float)> cb, bool integerOnly = false) {
		m_sliders.push_back({ pos, size, minVal, maxVal, initValue, std::move(cb), integerOnly });
		return *this;
	}

	Builder& Scale(float s) { m_scale = s; return *this; }

	std::shared_ptr<GameMessageBox> Show();   // 实现在 .cpp（注册到当前场景 UIManager）

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
	GameMessageBox::TooltipPanelConfig m_tooltipPanel;
};

#endif
