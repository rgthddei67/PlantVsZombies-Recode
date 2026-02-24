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
	const Vector DEFAULT_SIZE(SCENE_WIDTH / 2, SCENE_HEIGHT / 2);          // 无背景图片时的默认尺寸
	const Vector BASE_BUTTON_SIZE(125 * 0.8f, 52 * 0.8f);        // 按钮基准尺寸
	const int BASE_TITLE_FONT_SIZE = 20;
	const int BASE_MESSAGE_FONT_SIZE = 18;
	const int BASE_BUTTON_FONT_SIZE = 14;
	const int BUTTON_SPACING = 10;                 // 按钮间距基准
	const Vector TITLE_OFFSET = Vector(-70, -65);
	const Vector MESSAGE_OFFSET = Vector(-190, -25);
	const int BOTTOM_MARGIN = 10;                    // 按钮距底部边距
}

GameMessageBox::GameMessageBox(const Vector& pos,
	const std::string& message,
	const std::vector<ButtonConfig>& buttons,
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
{
	mIsUI = true;

	// 计算实际大小 = 原始尺寸 × 缩放
	Vector originalSize = GetBackgroundOriginalSize();
	m_size = originalSize * scale;
}

GameMessageBox::~GameMessageBox() {
	auto& ui = SceneManager::GetInstance().GetCurrectSceneUIManager();
	for (size_t i = 0; i < m_buttons.size(); i++)
	{
		ui.RemoveButton(m_buttons[i]);
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

	// 创建按钮
	for (const auto& config : m_buttonConfigs) {
		Vector btnSize = BASE_BUTTON_SIZE * m_scale;
		auto button = SceneManager::GetInstance().GetCurrectSceneUIManager().
			CreateButton(config.pos, btnSize);

		// 设置按钮文字（字体大小按比例缩放）
		int fontSize = static_cast<int>(BASE_BUTTON_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		button->SetTextColor(m_titleColor);
		button->SetHoverTextColor(m_titleColor);
		button->SetText(config.text, ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		button->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);

		// 设置回调（使用 weak_ptr 避免循环引用）
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
	}
}

void GameMessageBox::Draw(Graphics* g)
{
	if (!mActive) return;

	// 绘制背景（拉伸到缩放后的大小）
	if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const GLTexture* tex = resMgr.GetTexture(m_backgroundImageKey);
		Vector pos = g->ScreenToWorldPosition(m_position.x - 230, m_position.y - 180);
		g->DrawTexture(tex, pos.x, pos.y, m_size.x, m_size.y);
	}
	else {
		std::cerr << "[GameMessageBox::Draw] 你TM还不给我设置图片是吧" << std::endl;
	}

	// 绘制标题
	if (!m_title.empty()) {
		int fontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector titlePos = m_position + Vector(10 * m_scale + TITLE_OFFSET.x, TITLE_OFFSET.y);
		Vector pos2 = g->ScreenToWorldPosition(titlePos.x, titlePos.y);
		GameAPP::GetInstance().DrawText(m_title, pos2, m_titleColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	// 绘制消息
	if (!m_message.empty()) {
		int fontSize = static_cast<int>(BASE_MESSAGE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector msgPos = m_position + Vector(10 * m_scale + MESSAGE_OFFSET.x, MESSAGE_OFFSET.y);
		Vector pos3 = g->ScreenToWorldPosition(msgPos.x, msgPos.y);
		GameAPP::GetInstance().DrawText(m_message, pos3, m_textColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}
}

void GameMessageBox::Close()
{
	GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}
