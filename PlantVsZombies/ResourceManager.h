#pragma once
#ifndef _RESOURCEMANAGER_H
#define _RESOURCEMANAGER_H

#include "./Reanimation/Reanimation.h"
#include "./Reanimation/AnimationTypes.h"
#include "ResourcesXMLConfigReader.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

// OpenGL 纹理信息
struct GLTexture {
    GLuint id = 0;
    int width = 0;
    int height = 0;
};

class ResourceManager {
private:
    // OpenGL 纹理缓存
    std::unordered_map<std::string, GLTexture> mGLTextures;

    // 动画缓存
    std::unordered_map<std::string, std::shared_ptr<Reanimation>> mReanimations;

    // 字体缓存（按字体名 -> 大小 -> TTF_Font*）
    std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;

    // 音效缓存
    std::unordered_map<std::string, Mix_Chunk*> sounds;

    // 音乐缓存
    std::unordered_map<std::string, Mix_Music*> music;

    // 配置读取器
    ResourcesXMLConfigReader configReader;

    static ResourceManager* instance;
    ResourceManager() {}

    // 加载分割贴图
    bool LoadTiledTextureGL(const TiledImageInfo& info, const std::string& prefix);

public:
    static ResourceManager& GetInstance();
    static void ReleaseInstance();

    bool Initialize(const std::string& configPath = "./resources/resources.xml");

    // 加载纹理，返回纹理信息指针，失败返回 nullptr
    const GLTexture* LoadTexture(const std::string& filepath, const std::string& key = "");
    // 获取已加载的纹理，不存在返回 nullptr
    const GLTexture* GetTexture(const std::string& key) const;
    // 卸载单个纹理
    void UnloadTexture(const std::string& key);
    // 检查纹理是否存在
    bool HasTexture(const std::string& key) const;

    // 批量加载（使用配置信息）
    bool LoadAllGameImages();
    bool LoadAllParticleTextures();
    bool LoadAllFonts();
    bool LoadAllSounds();
    bool LoadAllMusic();
    bool LoadAllReanimations();

    // ---------- 动画管理 ----------
    std::shared_ptr<Reanimation> LoadReanimation(const std::string& key, const std::string& path);
    std::shared_ptr<Reanimation> GetReanimation(const std::string& key);
    void UnloadReanimation(const std::string& key);
    bool HasReanimation(const std::string& key) const;
    std::string AnimationTypeToString(AnimationType type);

    // ---------- 字体管理 ----------
    bool LoadFont(const std::string& path, const std::string& key = "");
    TTF_Font* GetFont(const std::string& key, int size);
    void UnloadFont(const std::string& key);
    void UnloadFontSize(const std::string& key, int size);
    void CleanupUnusedFontSizes();
    bool HasFont(const std::string& key) const;

    // ---------- 音效管理 ----------
    Mix_Chunk* LoadSound(const std::string& path, const std::string& key = "");
    Mix_Chunk* GetSound(const std::string& key);
    void UnloadSound(const std::string& key);
    bool HasSound(const std::string& key) const;

    // ---------- 音乐管理 ----------
    Mix_Music* LoadMusic(const std::string& path, const std::string& key = "");
    Mix_Music* GetMusic(const std::string& key);
    void UnloadMusic(const std::string& key);
    bool HasMusic(const std::string& key) const;

    // ---------- 辅助函数 ----------
    std::string GenerateStandardKey(const std::string& path, const std::string& prefix);
    std::vector<int> GetLoadedFontSizes(const std::string& key) const;
    int GetLoadedFontCount() const;

    // ---------- 获取配置信息（供外部使用）----------
    const std::vector<TiledImageInfo>& GetGameImageInfos() const { return configReader.GetGameImageInfos(); }
    const std::vector<TiledImageInfo>& GetParticleTextureInfos() const { return configReader.GetParticleTextureInfos(); }
    const std::vector<std::string>& GetSoundPaths() const { return configReader.GetSoundPaths(); }
    const std::vector<std::string>& GetMusicPaths() const { return configReader.GetMusicPaths(); }
    const std::unordered_map<std::string, std::string>& GetAnimationPaths() const { return configReader.GetReanimationPaths(); }

    // 清理所有资源
    void UnloadAll();

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};

#endif