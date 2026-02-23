#include "GameMessageBox.h"
#include "../ResourceManager.h"
#include "../GameAPP.h"
#include "../Game/GameObjectManager.h"
#include "InputHandler.h"
#include "../ResourceKeys.h"
#include "../Game/SceneManager.h"
#include <algorithm>
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
        SDL_Texture* tex = resMgr.GetTexture(m_backgroundImageKey);
        if (tex) {
            int w, h;
            SDL_QueryTexture(tex, nullptr, nullptr, &w, &h);
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
        // 设置按钮大小（基准大小 × 缩放）

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

void GameMessageBox::Draw(SDL_Renderer* renderer)
{
    if (!mActive) return;

    // 绘制背景（拉伸到缩放后的大小）
    if (!m_backgroundImageKey.empty()) {
        auto& resMgr = ResourceManager::GetInstance();
        if (resMgr.HasTexture(m_backgroundImageKey)) {
            SDL_Texture* tex = resMgr.GetTexture(m_backgroundImageKey);
            SDL_Rect dest = { (int)m_position.x - 230, (int)m_position.y - 180,
                              (int)m_size.x, (int)m_size.y };
            SDL_RenderCopy(renderer, tex, nullptr, &dest);
        }
        else {
            // 图片缺失时绘制半透明黑色矩形
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
            SDL_Rect rect = { (int)m_position.x, (int)m_position.y,
                              (int)m_size.x, (int)m_size.y };
            SDL_RenderFillRect(renderer, &rect);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }
    }
    else {
        // 默认背景：半透明黑色
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
        SDL_Rect rect = { (int)m_position.x, (int)m_position.y,
                          (int)m_size.x, (int)m_size.y };
        SDL_RenderFillRect(renderer, &rect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
    }

    // 绘制标题
    if (!m_title.empty()) {
        int fontSize = static_cast<int>(BASE_TITLE_FONT_SIZE * m_scale);
        if (fontSize < 8) fontSize = 8;
        Vector titlePos = m_position + Vector(10 * m_scale + TITLE_OFFSET.x, TITLE_OFFSET.y);
        GameAPP::GetInstance().DrawText(m_title, titlePos, m_titleColor,
            ResourceKeys::Fonts::FONT_FZCQ, fontSize);
    }

    // 绘制消息
    if (!m_message.empty()) {
        int fontSize = static_cast<int>(BASE_MESSAGE_FONT_SIZE * m_scale);
        if (fontSize < 8) fontSize = 8;
        Vector msgPos = m_position + Vector(10 * m_scale + MESSAGE_OFFSET.x, MESSAGE_OFFSET.y);
        GameAPP::GetInstance().DrawText(m_message, msgPos, m_textColor,
            ResourceKeys::Fonts::FONT_FZCQ, fontSize);
    }

    // 绘制按钮
    for (auto& button : m_buttons) {
        button->Draw(renderer);
    }
}

void GameMessageBox::Close()
{
    GameObjectManager::GetInstance().DestroyGameObject(shared_from_this());
}
