#include "GameMessageBox.h"
#include "../ResourceManager.h"
#include "../GameApp.h"
#include "InputHandler.h"
#include "UIManager.h"
#include "../ResourceKeys.h"
#include "../Game/SceneManager.h"
#include "../Logger.h"
#include <algorithm>
#include <memory>

namespace {
	const Vector DEFAULT_SIZE(SCENE_WIDTH / 2, SCENE_HEIGHT / 2);		// 默认位置
	const int BASE_TITLE_FONT_SIZE = 20;	// Title大小
	const int BASE_MESSAGE_FONT_SIZE = 18;
	const Vector TITLE_OFFSET = Vector(-70, -65);	// Title偏移
	const Vector MESSAGE_OFFSET = Vector(-190, -25);
}

GameMessageBox::GameMessageBox(UIManager* owner,
	const Vector& pos,
	const std::string& message,
	const std::vector<ButtonConfig>& buttons,
	const std::vector<SliderConfig>& sliders,
	const std::vector<TextConfig>& texts,
	const std::string& title,
	const std::string& backgroundImageKey,
	float scale,
	const Vector& explicitSize)
	: m_owner(owner)
	, m_position(pos)
	, m_scale(scale)
	, m_explicitSize(explicitSize)
	, m_title(title)
	, m_message(message)
	, m_backgroundImageKey(backgroundImageKey)
	, m_buttonConfigs(buttons)
	, m_sliderConfigs(sliders)
	, m_textConfigs(texts)
{
	Vector originalSize = GetBackgroundOriginalSize();
	if (explicitSize.x > 0.0f && explicitSize.y > 0.0f)
		m_size = explicitSize;                 // 自动决定大小：直接用调用方算好的尺寸
	else
		m_size = originalSize * scale;
	InitializeControls();
}

GameMessageBox::~GameMessageBox() {
	DetachControls();
}

Vector GameMessageBox::GetBackgroundOriginalSize() const
{
	if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const Texture* tex = resMgr.GetTexture(m_backgroundImageKey);
		if (tex) {
			int w, h;
			w = tex->width;
			h = tex->height;
			return Vector(static_cast<float>(w), static_cast<float>(h));
		}
	}
	return DEFAULT_SIZE;
}

void GameMessageBox::InitializeControls()
{
	if (!m_owner) return;

	for (const auto& config : m_buttonConfigs) {
		Vector btnSize = config.size * m_scale;

		auto button = m_owner->CreateButton(config.pos, btnSize);

		int fontSize = static_cast<int>(config.fontsize * m_scale);
		if (fontSize < 8) fontSize = 8;

		if (config.texture == ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX0 ||
			config.texture == ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX1) {
			button->SetAsCheckbox(true);
			button->SetImageKeys
			(config.texture, config.texture, config.texture,
				ResourceKeys::Textures::IMAGE_OPTIONS_CHECKBOX1);
			button->SetChecked(config.initChecked);
		}
		else {
			button->SetTextColor(m_titleColor);
			button->SetHoverTextColor(m_titleColor);
			button->SetText(config.text, ResourceKeys::Fonts::FONT_FZCQ, fontSize);
			button->SetAsCheckbox(false);
			button->SetImageKeys
			(config.texture, config.texture, config.texture, config.texture);
		}

		// UIManager 会在 ButtonManager 完成本帧遍历后处理关闭，回调期间 this 始终有效。
		button->SetClickCallBack([this, config](bool) {
			if (config.callback) config.callback();
			if (config.autoClose) {
				Close();
			}
			});
		button->SetEnabled(config.enabled);

		m_buttons.push_back(button);
		button->SetSkipDraw(true);
	}

	for (const auto& config : m_sliderConfigs) {
		auto slider = m_owner->CreateSlider
			(config.pos, config.size * m_scale, config.min, config.max, config.initValue);

		slider->SetIntegerOnly(config.integerOnly);

		slider->SetChangeCallBack([config](float value) {
			if (config.callback)
				config.callback(value);
			});

		m_sliders.push_back(slider);
		slider->SetSkipDraw(true);
	}
}

void GameMessageBox::Draw(Graphics* g)
{
	if (!m_active) return;

	const bool autoSized = (m_explicitSize.x > 0.0f && m_explicitSize.y > 0.0f);
	if (m_backgroundImageKey.empty() && autoSized) {
		// 自动尺寸 + 无纹理 → 画纯色面板：FillRect 与 DrawTexture 同坐标约定，
		// 面板矩形恰为 [m_position±m_size/2]，与按内容算好的文字/按钮坐标严格对齐。
		const float left = m_position.x - m_size.x / 2.0f;
		const float top  = m_position.y - m_size.y / 2.0f;
		const float bw   = 3.0f;   // 边框宽
		Vector outer = g->LogicalToWorld(left, top);
		Vector inner = g->LogicalToWorld(left + bw, top + bw);
		g->FillRect(outer.x, outer.y, m_size.x, m_size.y, glm::vec4(150, 170, 110, 255));                       // 边框（暖石绿）
		g->FillRect(inner.x, inner.y, m_size.x - bw * 2.0f, m_size.y - bw * 2.0f, glm::vec4(40, 42, 34, 236));   // 主体（深色半透明）
	}
	else if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const Texture* tex = resMgr.GetTexture(m_backgroundImageKey);
		// 自动尺寸模式以 m_position 为中心绘制；否则沿用固定 (230,180) 偏移
		Vector topLeft = autoSized
			? Vector(m_position.x - m_size.x / 2.0f, m_position.y - m_size.y / 2.0f)
			: Vector(m_position.x - 230, m_position.y - 180);
		Vector pos = g->LogicalToWorld(topLeft.x, topLeft.y);
		g->DrawTexture(tex, pos.x, pos.y, m_size.x, m_size.y);
	}
	else {
		LOG_WARN("UI") << "GameMessageBox::Draw 没有合适的绘制图片";
	}

	if (!m_title.empty()) {
		int fontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector titlePos = m_position + Vector(10 * m_scale + TITLE_OFFSET.x, TITLE_OFFSET.y);
		Vector pos2 = g->LogicalToWorld(titlePos.x, titlePos.y);
		GameAPP::GetInstance().DrawText(m_title, pos2, m_titleColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	if (!m_message.empty()) {
		int fontSize = static_cast<int>(BASE_MESSAGE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector msgPos = m_position + Vector(10 * m_scale + MESSAGE_OFFSET.x, MESSAGE_OFFSET.y);
		Vector pos3 = g->LogicalToWorld(msgPos.x, msgPos.y);
		GameAPP::GetInstance().DrawText(m_message, pos3, m_textColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	for (const auto& config : m_textConfigs) {
		int fontSize = static_cast<int>(config.size * m_scale);
		if (fontSize < 8) fontSize = 8;

		Vector pos4 = g->LogicalToWorld(config.pos.x, config.pos.y);
		GameAPP::GetInstance().DrawText(config.text, pos4, config.color,
			config.font, fontSize);
	}

	for (const auto& btn : m_buttons) {
		if (btn) btn->Draw(g);
	}
	for (const auto& slider : m_sliders) {
		if (slider) slider->Draw(g);
	}
}

void GameMessageBox::SetActive(bool active)
{
	if (active && m_closeRequested) return;

	m_active = active;
	for (size_t i = 0; i < m_buttons.size(); ++i) {
		if (m_buttons[i]) {
			const bool configuredEnabled = i < m_buttonConfigs.size() && m_buttonConfigs[i].enabled;
			m_buttons[i]->SetEnabled(active && configuredEnabled);
		}
	}
	for (const auto& slider : m_sliders) {
		if (slider) slider->SetDrag(active);
	}
}

void GameMessageBox::Close()
{
	if (m_closeRequested) return;
	SetActive(false);
	m_closeRequested = true;
}

void GameMessageBox::DetachControls()
{
	if (m_owner) {
		for (const auto& button : m_buttons) {
			if (button) m_owner->RemoveButton(button);
		}
		for (const auto& slider : m_sliders) {
			if (slider) m_owner->RemoveSlider(slider);
		}
	}
	m_buttons.clear();
	m_sliders.clear();
	m_owner = nullptr;
}

std::shared_ptr<GameMessageBox> GameMessageBox::Builder::Show()
{
	auto& ui = SceneManager::GetInstance().GetCurrectSceneUIManager();
	auto messageBox = std::make_shared<GameMessageBox>(&ui, m_pos, m_message,
		m_buttons, m_sliders, m_texts, m_title, m_bgKey, m_scale, m_explicitSize);
	ui.AddMessageBox(messageBox);
	return messageBox;
}
