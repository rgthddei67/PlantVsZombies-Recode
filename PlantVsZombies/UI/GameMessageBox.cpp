#include "GameMessageBox.h"
#include "../ResourceManager.h"
#include "../GameAPP.h"
#include "../Game/GameObjectManager.h"
#include "InputHandler.h"
#include "../ResourceKeys.h"
#include "../Game/SceneManager.h"
#include <algorithm>
#include <iostream>
#include <memory>

namespace {
	const Vector DEFAULT_SIZE(SCENE_WIDTH / 2, SCENE_HEIGHT / 2);		// 默认位置	     
	const int BASE_TITLE_FONT_SIZE = 20;	// Title大小
	const int BASE_MESSAGE_FONT_SIZE = 18;	
	const int BUTTON_SPACING = 10;			// 按钮距离上面偏移
	const Vector TITLE_OFFSET = Vector(-70, -65);	// Title偏移
	const Vector MESSAGE_OFFSET = Vector(-190, -25);
	const int BOTTOM_MARGIN = 10;              
}

GameMessageBox::GameMessageBox(const Vector& pos,
	const std::string& message,
	const std::vector<ButtonConfig>& buttons,
	const std::vector<SliderConfig>& sliders,
	const std::vector<TextConfig>& texts,
	const std::string& title,
	const std::string& backgroundImageKey,
	float scale)
	: GameObject(ObjectType::OBJECT_UI)
	, m_position(pos)
	, m_scale(scale)
	, m_title(title)
	, m_message(message)
	, m_backgroundImageKey(backgroundImageKey)
	, m_buttonConfigs(buttons)
	, m_sliderConfigs(sliders)
	, m_textConfigs(texts)
{
	mIsUI = true;
	
	Vector originalSize = GetBackgroundOriginalSize();
	m_size = originalSize * scale;
	this->SetRenderOrder(LAYER_UI + 500000);
}

GameMessageBox::~GameMessageBox() {
	auto& ui = SceneManager::GetInstance().GetCurrectSceneUIManager();
	for (size_t i = 0; i < m_buttons.size(); i++)
	{
		auto button = m_buttons[i];
		if (button)
			ui.RemoveButton(button);
	}

	for (size_t i = 0; i < m_sliders.size(); i++)
	{
		auto slider = m_sliders[i];
		if (slider)
			ui.RemoveSlider(slider);
	}
}

Vector GameMessageBox::GetBackgroundOriginalSize() const
{
	if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const GLTexture* tex = resMgr.GetTexture(m_backgroundImageKey);
		if (tex) {
			int w, h;
			w = tex->width;
			h = tex->height;
			return Vector(static_cast<float>(w), static_cast<float>(h));
		}
	}
	return DEFAULT_SIZE;
}

void GameMessageBox::Start()
{
	GameObject::Start();

	for (const auto& config : m_buttonConfigs) {
		Vector btnSize = config.size * m_scale;
		
		auto button = SceneManager::GetInstance().GetCurrectSceneUIManager().
			CreateButton(config.pos, btnSize);

		int fontSize = static_cast<int>(config.fontsize * m_scale);
		if (fontSize < 8) fontSize = 8;

		button->SetAsCheckbox(false);
		button->SetTextColor(m_titleColor);
		button->SetHoverTextColor(m_titleColor);
		button->SetText(config.text, ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		button->SetImageKeys
			(config.texture, config.texture, config.texture, config.texture);

		// 使用weakptr 避免循环引用
		auto weakSelf = std::weak_ptr<GameMessageBox>(std::dynamic_pointer_cast<GameMessageBox>(shared_from_this()));
		button->SetClickCallBack([weakSelf, config](bool) {
			if (config.callback) config.callback();
			if (config.autoClose) {
				if (auto self = weakSelf.lock()) {
					self->Close();
				}
			}
			});

		m_buttons.push_back(button);
		button->SetSkipDraw(true);
	}

	for (const auto& config : m_sliderConfigs) {
		auto slider = SceneManager::GetInstance().GetCurrectSceneUIManager().
			CreateSlider
			(config.pos, config.size * m_scale, config.min, config.max, config.initValue);

		auto weakSelf = std::weak_ptr<GameMessageBox>(std::dynamic_pointer_cast<GameMessageBox>(shared_from_this()));
		slider->SetChangeCallBack([weakSelf, config](float value) {
			if (config.callback)
				config.callback(value);
			});

		m_sliders.push_back(slider);
		slider->SetSkipDraw(true);
	}
}

void GameMessageBox::Draw(Graphics* g)
{
	if (!mActive) return;

	if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const GLTexture* tex = resMgr.GetTexture(m_backgroundImageKey);
		Vector pos = g->ScreenToWorldPosition(m_position.x - 230, m_position.y - 180);
		g->DrawTexture(tex, pos.x, pos.y, m_size.x, m_size.y);
	}
	else {
		std::cerr << "[GameMessageBox::Draw] 没有合适的绘制图片" << std::endl;
	}

	if (!m_title.empty()) {
		int fontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector titlePos = m_position + Vector(10 * m_scale + TITLE_OFFSET.x, TITLE_OFFSET.y);
		Vector pos2 = g->ScreenToWorldPosition(titlePos.x, titlePos.y);
		GameAPP::GetInstance().DrawText(m_title, pos2, m_titleColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	if (!m_message.empty()) {
		int fontSize = static_cast<int>(BASE_MESSAGE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector msgPos = m_position + Vector(10 * m_scale + MESSAGE_OFFSET.x, MESSAGE_OFFSET.y);
		Vector pos3 = g->ScreenToWorldPosition(msgPos.x, msgPos.y);
		GameAPP::GetInstance().DrawText(m_message, pos3, m_textColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	for (const auto& config : m_textConfigs) {
		int fontSize = static_cast<int>(config.size * m_scale);
		if (fontSize < 8) fontSize = 8;

		Vector pos4 = g->ScreenToWorldPosition(config.pos.x, config.pos.y);
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

void GameMessageBox::Close()
{
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}
