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
        [this](Graphics* g) { this->DrawAllTextures(g); },
        LAYER_BACKGROUND);

    // 添加游戏对象绘制命令
    RegisterDrawCommand("GameObjects",
        [this](Graphics* g) { this->DrawGameObjects(g); },
        LAYER_GAME_OBJECT);

    // 注册粒子系统绘制
    RegisterDrawCommand("ParticleSystem",
        [](Graphics* g) {
            if (g_particleSystem) {
                g_particleSystem->DrawAll();
            }
        },
        LAYER_EFFECTS);

    // 添加UI绘制命令
    RegisterDrawCommand("UI",
        [this](Graphics* g) { this->mUIManager.DrawAll(g); },
        LAYER_UI);
}

void Scene::RegisterDrawCommand(const std::string& name,
    std::function<void(Graphics*)> drawFunc,
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

void Scene::Draw(Graphics* g) {
    if (!mDrawCommandsBuilt) {
        BuildDrawCommands();
        mDrawCommandsBuilt = true;
    }
    for (auto& cmd : mDrawCommands) {
        if (cmd.drawFunc) {
            cmd.drawFunc(g);
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

void Scene::AddTexture(const std::string& textureName, float posX, float posY, float scaleX, float scaleY, int drawOrder, bool isUI) {
    const GLTexture* texture = ResourceManager::GetInstance().GetTexture(textureName);
    if (texture) {
        TextureInfo info{ texture, posX, posY };
        info.scaleX = scaleX;
        info.scaleY = scaleY;
        info.drawOrder = drawOrder;
        info.name = textureName;              
        info.isUI = isUI;                   
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

void Scene::DrawAllTextures(Graphics* g) {
    // 按绘制顺序排序
    std::sort(mTextures.begin(), mTextures.end(),
        [](const TextureInfo& a, const TextureInfo& b) {
            return a.drawOrder < b.drawOrder;
        });

    for (size_t i = 0; i < mTextures.size(); ++i) {
        const auto& texInfo = mTextures[i];
        if (!texInfo.visible) continue;

        // 计算显示尺寸
        float displayWidth = texInfo.texture->width * texInfo.scaleX;
        float displayHeight = texInfo.texture->height * texInfo.scaleY;
        Vector drawPosition = Vector(texInfo.posX, texInfo.posY);

        if (texInfo.isUI) {
            drawPosition = g->ScreenToWorldPosition(texInfo.posX, texInfo.posY);
        }

        g->DrawTexture(texInfo.texture, drawPosition.x, drawPosition.y,
            displayWidth, displayHeight);
    }
}

TextureInfo* Scene::GetTextureInfo(const std::string& textureName) {
    auto it = std::find_if(mTextures.begin(), mTextures.end(),
        [&textureName](const TextureInfo& info) {
            return info.name == textureName;
        });

    return it != mTextures.end() ? &(*it) : nullptr;
}