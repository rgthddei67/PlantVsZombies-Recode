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
	const Vector DEFAULT_SIZE(SCENE_WIDTH / 2, SCENE_HEIGHT / 2);          // 鏃犺儗鏅浘鐗囨椂鐨勯粯璁ゅ昂瀵?
	const Vector BASE_BUTTON_SIZE(125 * 0.8f, 52 * 0.8f);        // 鎸夐挳鍩哄噯灏哄
	const int BASE_TITLE_FONT_SIZE = 20;
	const int BASE_MESSAGE_FONT_SIZE = 18;
	const int BASE_BUTTON_FONT_SIZE = 14;
	const int BUTTON_SPACING = 10;                 // 鎸夐挳闂磋窛鍩哄噯
	const Vector TITLE_OFFSET = Vector(-70, -65);
	const Vector MESSAGE_OFFSET = Vector(-190, -25);
	const int BOTTOM_MARGIN = 10;                    // 鎸夐挳璺濆簳閮ㄨ竟璺?
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

	// 璁＄畻瀹為檯澶у皬 = 鍘熷灏哄 脳 缂╂斁
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

	// 鍒涘缓鎸夐挳
	for (const auto& config : m_buttonConfigs) {
		Vector btnSize = BASE_BUTTON_SIZE * m_scale;
		auto button = SceneManager::GetInstance().GetCurrectSceneUIManager().
			CreateButton(config.pos, btnSize);

		// 璁剧疆鎸夐挳鏂囧瓧锛堝瓧浣撳ぇ灏忔寜姣斾緥缂╂斁锛?
		int fontSize = static_cast<int>(BASE_BUTTON_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		button->SetTextColor(m_titleColor);
		button->SetHoverTextColor(m_titleColor);
		button->SetText(config.text, ResourceKeys::Fonts::FONT_FZCQ, fontSize);
		button->SetImageKeys(ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL,
			ResourceKeys::Textures::IMAGE_BUTTONSMALL, ResourceKeys::Textures::IMAGE_BUTTONSMALL);

		// 璁剧疆鍥炶皟锛堜娇鐢?weak_ptr 閬垮厤寰幆寮曠敤锛?
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

	// 缁樺埗鑳屾櫙锛堟媺浼稿埌缂╂斁鍚庣殑澶у皬锛?
	if (!m_backgroundImageKey.empty()) {
		auto& resMgr = ResourceManager::GetInstance();
		const GLTexture* tex = resMgr.GetTexture(m_backgroundImageKey);
		Vector pos = g->ScreenToWorldPosition(m_position.x - 230, m_position.y - 180);
		g->DrawTexture(tex, pos.x, pos.y, m_size.x, m_size.y);
	}
	else {
		std::cerr << "[GameMessageBox::Draw] 浣燭M杩樹笉缁欐垜璁剧疆鍥剧墖鏄惂" << std::endl;
	}

	// 缁樺埗鏍囬
	if (!m_title.empty()) {
		int fontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * m_scale);
		if (fontSize < 8) fontSize = 8;
		Vector titlePos = m_position + Vector(10 * m_scale + TITLE_OFFSET.x, TITLE_OFFSET.y);
		Vector pos2 = g->ScreenToWorldPosition(titlePos.x, titlePos.y);
		GameAPP::GetInstance().DrawText(m_title, pos2, m_titleColor,
			ResourceKeys::Fonts::FONT_FZCQ, fontSize);
	}

	// 缁樺埗娑堟伅
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
