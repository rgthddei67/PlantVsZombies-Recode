#pragma once
#ifndef _RESOURCEMANAGER_H
#define _RESOURCEMANAGER_H
#include "AllCppInclude.h"
#include "Reanimator.h"
#include "ReanimParser.h"
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>

class ReanimatorDefinition;

// TODO :������Դ������
enum class AnimationType
{
    ANIM_NONE,
    ANIM_SUN,
};


class ResourceManager
{
private:

    std::unordered_map<std::string, SDL_Texture*> textures;
    std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;
    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, Mix_Music*> music;
    std::unordered_map<AnimationType, std::shared_ptr<ReanimatorDefinition>> animations;
    std::unordered_map<AnimationType, std::string> animationPaths;

    SDL_Renderer* renderer;

    static ResourceManager* instance;
    ResourceManager() : renderer(nullptr) 
    {
        InitializeAnimationPaths();
    }

    void InitializeAnimationPaths() {
        animationPaths = {
            {AnimationType::ANIM_SUN, "./resources/reanim/Sun"},
        };
    }

    // ��Դ·������
    std::vector<std::string> gameImagePaths = {
        "./resources/image/kid.jpg",
        "./resources/image/UI/options_checkbox0.png",
        "./resources/image/UI/options_checkbox1.png",
        "./resources/image/UI/SeedChooser_Button2.png",
        "./resources/image/UI/SeedChooser_Button2_Glow.png",
        "./resources/image/UI/options_sliderknob2.png",
        "./resources/image/UI/options_sliderslot.png"
    };

    std::vector<std::string> particleTexturePaths = {
        "./resources/particles/ZombieHead.png",
    };

    std::vector<std::string> fontPaths = {
        "./font/fzcq.ttf",
        "./font/fzjz.ttf",
        "./font/ContinuumBold.ttf"
    };

    std::vector<std::string> soundPaths = {
        "./resources/sounds/UI/bleep.ogg",
        "./resources/sounds/UI/buttonclick.ogg",
        "./resources/sounds/UI/ceramic.ogg",
        "./resources/sounds/UI/chooseplant1.ogg",
        "./resources/sounds/UI/chooseplant2.ogg",
        "./resources/sounds/UI/gravebutton.ogg",
        "./resources/sounds/UI/mainmenu.ogg",
        "./resources/sounds/UI/pause.ogg",
        "./resources/sounds/UI/shovel.ogg",
    };

    std::vector<std::string> musicPaths = {
        "./resources/music/MainMenu.ogg",
    };

public:
    // ��������
    static ResourceManager& GetInstance();
    static void ReleaseInstance();

    // ͳһ���ط���
    bool LoadAllGameImages();
    bool LoadAllParticleTextures();
    bool LoadAllFonts();
    bool LoadAllSounds();
    bool LoadAllMusic();
    bool LoadAllAnimations();

    // ��ȡ��Դ��
    const std::vector<std::string>& GetGameImagePaths() const;
    const std::vector<std::string>& GetParticleTexturePaths() const;
    const std::vector<std::string>& GetSoundPaths() const;
    const std::vector<std::string>& GetMusicPaths() const;
    const std::unordered_map<AnimationType, std::string>& GetAnimationPaths() const { return animationPaths; }

    // ��ʼ��
    void Initialize(SDL_Renderer* renderer);

    // �������
    SDL_Texture* LoadTexture(const std::string& path, const std::string& key = "");
    SDL_Texture* GetTexture(const std::string& key);
    void UnloadTexture(const std::string& key);

    // �������
    bool LoadFont(const std::string& path, const std::string& key = "");
    TTF_Font* GetFont(const std::string& key, int size);
    void UnloadFont(const std::string& key);
    void UnloadFontSize(const std::string& key, int size);
    void CleanupUnusedFontSizes();
    // ��ȡ������Ϣ
    std::vector<int> GetLoadedFontSizes(const std::string& key) const;
    int GetLoadedFontCount() const;

    // ��Ч����
    Mix_Chunk* LoadSound(const std::string& path, const std::string& key = "");
    Mix_Chunk* GetSound(const std::string& key);
    void UnloadSound(const std::string& key);

    // ���ֹ���
    Mix_Music* LoadMusic(const std::string& path, const std::string& key = "");
    Mix_Music* GetMusic(const std::string& key);
    void UnloadMusic(const std::string& key);

    // ��������
    bool LoadAnimation(AnimationType animType);
    std::shared_ptr<ReanimatorDefinition> GetAnimation(AnimationType animType);
    void UnloadAnimation(AnimationType animType);

    // ��������
    void LoadTexturePack(const std::vector<std::pair<std::string, std::string>>& texturePaths);
    void UnloadAll();

    // ���߷���
    bool HasTexture(const std::string& key) const;
    bool HasFont(const std::string& key) const;
    bool HasSound(const std::string& key) const;
    bool HasMusic(const std::string& key) const;
    bool HasAnimation(AnimationType animType) const;

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};

#endif