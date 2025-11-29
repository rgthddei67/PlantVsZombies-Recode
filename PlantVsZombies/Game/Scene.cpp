#include "Scene.h"
#include "../ResourceManager.h"
#include <iostream>

void Scene::AddTexture(const std::string& textureName, int posX, int posY, float scaleX, float scaleY, int drawOrder) {
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

void Scene::SetTexturePosition(const std::string& textureName, int posX, int posY) {
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
    for (const auto& texInfo : mTextures) {
        if (texInfo.texture && texInfo.visible) {
            int texWidth, texHeight;
            SDL_QueryTexture(texInfo.texture, nullptr, nullptr, &texWidth, &texHeight);

            int displayWidth = static_cast<int>(texWidth * texInfo.scaleX);
            int displayHeight = static_cast<int>(texHeight * texInfo.scaleY);

            SDL_Rect destRect = {
                texInfo.posX,
                texInfo.posY,
                displayWidth,
                displayHeight
            };

            SDL_RenderCopy(renderer, texInfo.texture, nullptr, &destRect);
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