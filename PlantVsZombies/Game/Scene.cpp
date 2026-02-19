#include "Scene.h"
#include "GameObjectManager.h"
#include "CollisionSystem.h"
#include "../ResourceManager.h"
#include "../ParticleSystem/ParticleSystem.h"
#include "ClickableComponent.h"
#include "../GameApp.h"
#include <iostream>

void Scene::BuildDrawCommands() {
    mDrawCommands.clear();

	// 添加纹理绘制命令
    RegisterDrawCommand("GameTextures",
        [this](SDL_Renderer* renderer) { this->DrawAllTextures(renderer); },
        LAYER_BACKGROUND);

    // 添加游戏对象绘制命令
    RegisterDrawCommand("GameObjects",
        [this](SDL_Renderer* renderer) { this->DrawGameObjects(renderer); },
        LAYER_GAME_OBJECT);

    // 注册粒子系统绘制
    RegisterDrawCommand("ParticleSystem",
        [](SDL_Renderer* renderer) {
            if (g_particleSystem) {
                g_particleSystem->DrawAll();
            }
        },
        LAYER_EFFECTS);

    // 添加UI绘制命令
    RegisterDrawCommand("UI",
        [this](SDL_Renderer* renderer) { this->mUIManager.DrawAll(renderer); },
        LAYER_UI);
}

void Scene::RegisterDrawCommand(const std::string& name,
    std::function<void(SDL_Renderer*)> drawFunc,
    int renderOrder) 
{
    auto it = std::find_if(mDrawCommands.begin(), mDrawCommands.end(),
        [&name](const DrawCommand& cmd) { return cmd.name == name; });

    if (it != mDrawCommands.end()) {
        it->drawFunc = drawFunc;
        it->renderOrder = renderOrder;
    }
    else {
        mDrawCommands.emplace_back(drawFunc, renderOrder, name);
    }
}

void Scene::Draw(SDL_Renderer* renderer) {
    if (!mDrawCommandsBuilt) {
        BuildDrawCommands();
        mDrawCommandsBuilt = true;
    }
    for (auto& cmd : mDrawCommands) {
        if (cmd.drawFunc) {
            cmd.drawFunc(renderer);
        }
    }
}

void Scene::Update()
{
    if (g_particleSystem) 
    {
        g_particleSystem->UpdateAll();
    }
    auto input = &GameAPP::GetInstance().GetInputHandler();
	mUIManager.ProcessMouseEvent(input);
	mUIManager.UpdateAll(input);
    GameObjectManager::GetInstance().Update();
    ClickableComponent::ProcessMouseEvents();
    CollisionSystem::GetInstance().Update();
}

void Scene::UnregisterDrawCommand(const std::string& name) {
    auto it = std::remove_if(mDrawCommands.begin(), mDrawCommands.end(),
        [&name](const DrawCommand& cmd) { return cmd.name == name; });
    mDrawCommands.erase(it, mDrawCommands.end());
}

void Scene::AddTexture(const std::string& textureName, float posX, float posY, float scaleX, float scaleY, int drawOrder) {
    SDL_Texture* texture = ResourceManager::GetInstance().GetTexture(textureName);
    if (texture) {
        TextureInfo info(texture, posX, posY, textureName);
        info.scaleX = scaleX;
        info.scaleY = scaleY;
        info.drawOrder = drawOrder;
        mTextures.push_back(info);
#ifdef _DEBUG
        std::cout << "场景 " << name << " 添加纹理: " << textureName
            << " 位置: (" << posX << ", " << posY << ")"
            << " 缩放X:" << scaleX << "Y:" << scaleY << std::endl;
#endif
    }
    else {
        std::cerr << "场景 " << name << " 无法加载纹理: " << textureName << std::endl;
    }
}

void Scene::RemoveTexture(const std::string& textureName) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
#ifdef _DEBUG
        std::cout << "场景 " << name << " 移除纹理: " << textureName << std::endl;
#endif
        mTextures.erase(it);
    }
}

void Scene::SetTexturePosition(const std::string& textureName, float posX, float posY) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->posX = posX;
        it->posY = posY;
    }
}

void Scene::SetTextureScale(const std::string& textureName, float scaleX, float scaleY) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->scaleX = scaleX;
        it->scaleY = scaleY;
    }
}

void Scene::SetTextureScaleX(const std::string& textureName, float scaleX) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->scaleX = scaleX;
    }
}

void Scene::SetTextureScaleY(const std::string& textureName, float scaleY) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->scaleY = scaleY;
    }
}

void Scene::SetTextureVisible(const std::string& textureName, bool visible) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->visible = visible;
    }
}

void Scene::SetTextureDrawOrder(const std::string& textureName, int drawOrder) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    if (it != mTextures.end()) {
        it->drawOrder = drawOrder;
    }
}

void Scene::ClearAllTextures() {
#ifdef _DEBUG
    std::cout << "场景 " << name << " 清空所有纹理" << std::endl;
#endif
    mTextures.clear();
}

void Scene::DrawAllTextures(SDL_Renderer* renderer) {
    // 按绘制顺序排序
    std::sort(mTextures.begin(), mTextures.end(),
        [](const TextureInfo& a, const TextureInfo& b) {
            return a.drawOrder < b.drawOrder;
        });

    // 绘制所有可见纹理
    for (size_t i = 0; i < mTextures.size(); i++)
    {
		auto texInfo = mTextures[i];
        if (texInfo.texture && texInfo.visible) {
            int texWidth, texHeight;
            SDL_QueryTexture(texInfo.texture, nullptr, nullptr, &texWidth, &texHeight);

            float displayWidth = texWidth * texInfo.scaleX;
            float displayHeight = texHeight * texInfo.scaleY;

            SDL_FRect destRect = {
                texInfo.posX,
                texInfo.posY,
                displayWidth,
                displayHeight
            };

            SDL_RenderCopyF(renderer, texInfo.texture, nullptr, &destRect);
        }
    }
}

TextureInfo* Scene::GetTextureInfo(const std::string& textureName) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    return it != mTextures.end() ? &(*it) : nullptr;
}