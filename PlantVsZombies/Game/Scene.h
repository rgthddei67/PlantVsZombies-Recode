#pragma once
#ifndef _SCENE_H
#define _SCENE_H

#include "../UI/UIManager.h"
#include "../ResourceKeys.h"
#include "./GameObjectManager.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// 绘制命令结构
struct DrawCommand {
    std::function<void(SDL_Renderer*)> drawFunc;
    int renderOrder;
    std::string name;

    DrawCommand(std::function<void(SDL_Renderer*)> func = nullptr,
        int order = 0,
        const std::string& n = "")
        : drawFunc(func), renderOrder(order), name(n) {
    }

    bool operator<(const DrawCommand& other) const {
        return renderOrder < other.renderOrder;
    }
};

// 图片信息结构
struct TextureInfo {
    SDL_Texture* texture;
    int posX;
    int posY;
    float scaleX;       // X轴缩放比例（默认为1.0）
    float scaleY;       // Y轴缩放比例（默认为1.0）
    bool visible;       // 是否可见
    int drawOrder;      // 绘制顺序（数值小的先绘制）
    std::string name;   // 纹理名称

    TextureInfo(SDL_Texture* tex = nullptr, int x = 0, int y = 0, const std::string& n = "")
        : texture(tex), posX(x), posY(y), scaleX(1.0f), scaleY(1.0f), visible(true), drawOrder(0), name(n) {
    }
};

class Scene 
{
public:
    std::string name;

    virtual ~Scene() = default;

    virtual void OnEnter()  // 进入场景时调用
    {
        RebuildDrawCommands();
    }   

    virtual void OnExit()    // 退出场景时调用  
    {
        mUIManager.ClearAll();
        GameObjectManager::GetInstance().DestroyAllGameObjects();
        mDrawCommands.clear();
    }    

    virtual void Update();

    virtual void Draw(SDL_Renderer* renderer);

    // 注册自定义绘制命令
    void RegisterDrawCommand(const std::string& name,
        std::function<void(SDL_Renderer*)> drawFunc,
        int renderOrder = LAYER_GAME_OBJECT);

    // 移除绘制命令
    void UnregisterDrawCommand(const std::string& name);

    // 重构绘制命令
    void RebuildDrawCommands() {
        mDrawCommandsBuilt = false;
    }

protected:
    UIManager mUIManager;
    bool mDrawCommandsBuilt = false;

    // 构建绘制命令
    virtual void BuildDrawCommands();

    // 绘制所有游戏对象
    virtual void DrawGameObjects(SDL_Renderer* renderer) {
        GameObjectManager::GetInstance().DrawAll(renderer);
    }

    // 按渲染顺序排序绘制命令（构建完成后调用）
    void SortDrawCommands() {
        std::sort(mDrawCommands.begin(), mDrawCommands.end());
    }

    // 添加纹理到绘制列表
    void AddTexture(const std::string& textureName, int posX, int posY, float scaleX = 1.0f, float scaleY = 1.0f, int drawOrder = 0);

    // 从绘制列表移除纹理
    void RemoveTexture(const std::string& textureName);

    // 更新纹理位置
    void SetTexturePosition(const std::string& textureName, int posX, int posY);

    // 设置纹理缩放
    void SetTextureScale(const std::string& textureName, float scaleX, float scaleY);
    
    // 设置纹理缩放X
    void SetTextureScaleX(const std::string& textureName, float scaleX);

    // 设置纹理缩放Y
    void SetTextureScaleY(const std::string& textureName, float scaleY);

    // 设置纹理可见性
    void SetTextureVisible(const std::string& textureName, bool visible);

    // 设置纹理绘制顺序
    void SetTextureDrawOrder(const std::string& textureName, int drawOrder);

    // 清空所有纹理
    void ClearAllTextures();

    // 绘制所有纹理
    void DrawAllTextures(SDL_Renderer* renderer);

    // 获取纹理信息（用于调试或特殊处理）
    TextureInfo* GetTextureInfo(const std::string& textureName);

private:
    std::vector<TextureInfo> mTextures;
    std::vector<DrawCommand> mDrawCommands;
};

#endif