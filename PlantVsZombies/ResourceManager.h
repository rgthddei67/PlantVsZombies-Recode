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
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

class Animator;

class ResourceManager
{
private:
    std::unordered_map<std::string, std::shared_ptr<Reanimation>> mReanimations;  // 已加载的动画
    std::unordered_map<std::string, SDL_Texture*> textures;
    std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;
    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, Mix_Music*> music;

    SDL_Renderer* renderer;
    ResourcesXMLConfigReader configReader;

    static ResourceManager* instance;
    ResourceManager() : renderer(nullptr) 
    {
    }

public:
    // 单例访问
    static ResourceManager& GetInstance();
    static void ReleaseInstance();

    bool Initialize(SDL_Renderer* renderer, const std::string& configPath = "./resources/resources.xml");

    // 统一加载方法
    bool LoadAllGameImages();
    bool LoadAllParticleTextures();
    bool LoadAllFonts();
    bool LoadAllSounds();
    bool LoadAllMusic();
    bool LoadAllReanimations();

    // 获取资源组
    const std::vector<std::string>& GetGameImagePaths() const { return configReader.GetGameImagePaths(); }
    const std::vector<std::string>& GetParticleTexturePaths() const { return configReader.GetParticleTexturePaths(); }
    const std::vector<std::string>& GetSoundPaths() const { return configReader.GetSoundPaths(); }
    const std::vector<std::string>& GetMusicPaths() const { return configReader.GetMusicPaths(); }
    const std::unordered_map<std::string, std::string>& GetAnimationPaths() const { return configReader.GetReanimationPaths(); }

    // 纹理管理
    SDL_Texture* LoadTexture(const std::string& path, const std::string& key = "");
    SDL_Texture* GetTexture(const std::string& key);
    void UnloadTexture(const std::string& key);

    std::shared_ptr<Reanimation> LoadReanimation(const std::string& key, const std::string& path);
    std::shared_ptr<Reanimation> GetReanimation(const std::string& key);
    std::string AnimationTypeToString(AnimationType type);
    void UnloadReanimation(const std::string& key);

    // 字体管理
    bool LoadFont(const std::string& path, const std::string& key = "");
    TTF_Font* GetFont(const std::string& key, int size);
    void UnloadFont(const std::string& key);
    void UnloadFontSize(const std::string& key, int size);
    void CleanupUnusedFontSizes();

    // 获取字体信息
    std::vector<int> GetLoadedFontSizes(const std::string& key) const;
    int GetLoadedFontCount() const;

    // 音效管理
    Mix_Chunk* LoadSound(const std::string& path, const std::string& key = "");
    Mix_Chunk* GetSound(const std::string& key);
    void UnloadSound(const std::string& key);

    // 音乐管理
    Mix_Music* LoadMusic(const std::string& path, const std::string& key = "");
    Mix_Music* GetMusic(const std::string& key);
    void UnloadMusic(const std::string& key);

    // 批量操作
    void LoadTexturePack(const std::vector<std::pair<std::string, std::string>>& texturePaths);
    void UnloadAll();

    bool HasTexture(const std::string& key) const;
    bool HasFont(const std::string& key) const;
    bool HasSound(const std::string& key) const;
    bool HasMusic(const std::string& key) const;
    bool HasReanimation(const std::string& key) const;

    // 根据路径生成标准化的key
    std::string GenerateTextureKey(const std::string& path);
    // 大写的
    std::string GenerateStandardKey(const std::string& path, const std::string& prefix);

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};

#endif