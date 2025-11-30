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
    std::unordered_map<std::string, std::string> mReanimationPaths;  // 动画路径
    std::unordered_map<std::string, std::shared_ptr<Reanimation>> mReanimations;  // 已加载的动画
    std::unordered_map<std::string, SDL_Texture*> textures;
    std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;
    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, Mix_Music*> music;

    SDL_Renderer* renderer;

    static ResourceManager* instance;
    ResourceManager() : renderer(nullptr) 
    {
        InitializeAnimationPaths();
    }

    // TODO:新增资源改这里
    void InitializeAnimationPaths() {
        mReanimationPaths = {
            {"Sun", "./resources/reanim/Sun.reanim"},
        };
    }
public:
    std::string AnimationTypeToString(AnimationType type) {
        switch (type) {
        case AnimationType::ANIM_SUN:
            return "Sun";
        case AnimationType::ANIM_NONE:
        default:
            return "Unknown";
        }
    }
private:

    // 资源路径配置 直接在后面加 不要改顺序吧
    std::vector<std::string> gameImagePaths = {
        "./resources/image/kid.jpg",
        "./resources/image/UI/options_checkbox0.png",
        "./resources/image/UI/options_checkbox1.png",
        "./resources/image/UI/SeedChooser_Button2.png",
        "./resources/image/UI/SeedChooser_Button2_Glow.png",
        "./resources/image/UI/options_sliderknob2.png",
        "./resources/image/UI/options_sliderslot.png",
        "./resources/image/UI/SelectorScreen_Adventure_button.png",
        "./resources/image/UI/SelectorScreen_Adventure_highlight.png",
        "./resources/image/UI/SelectorScreen_Almanac.png",
        "./resources/image/UI/SelectorScreen_AlmanacHighlight.png",
        "./resources/image/UI/SelectorScreen_BG.png",
        "./resources/image/UI/SelectorScreen_BG_Center.png",
        "./resources/image/UI/SelectorScreen_BG_Left.png",
        "./resources/image/UI/SelectorScreen_BG_Right.png",
        "./resources/image/UI/SelectorScreen_Challenges_button.png",
        "./resources/image/UI/SelectorScreen_Challenges_highlight.png",
        "./resources/image/UI/SelectorScreen_Flower1.png",
        "./resources/image/UI/SelectorScreen_Flower2.png",
        "./resources/image/UI/SelectorScreen_Flower3.png",
        "./resources/image/UI/SelectorScreen_Game.png",
        "./resources/image/UI/SelectorScreen_Game_Shadow.png",
        "./resources/image/UI/SelectorScreen_Help1.png",
        "./resources/image/UI/SelectorScreen_Help2.png",
        "./resources/image/UI/SelectorScreen_Options1.png",
        "./resources/image/UI/SelectorScreen_Options2.png",
        "./resources/image/UI/SelectorScreen_Quit1.png",
        "./resources/image/UI/SelectorScreen_Quit2.png",
        "./resources/image/UI/SelectorScreen_StartAdventure_Button1.png",
        "./resources/image/UI/SelectorScreen_StartAdventure_Highlight.png",
        "./resources/image/UI/SelectorScreen_Store.png",
        "./resources/image/UI/SelectorScreen_StoreHighlight.png",
        "./resources/image/UI/SelectorScreen_Surival.png",
        "./resources/image/UI/SelectorScreen_Surival_Shadow.png",
        "./resources/image/UI/card_bk.jpg",
        "./resources/image/UI/SeedBank_Long.png",
        "./resources/image/UI/SeedChooser_Background.png",
        "./resources/image/UI/SeedChooser_Button.png",
        "./resources/image/UI/SeedChooser_Button_Disabled.png",
        "./resources/image/UI/SeedPacketNormal.png",
        "./resources/image/background_day.jpg",
        "./resources/image/background_night.jpg",
        "./resources/image/UI/ShovelBank.png",
        "./resources/image/UI/options_menuback.png",
        "./resources/image/UI/buttonbig.png",
        "./resources/image/UI/buttonsmall.png",
        "./resources/image/UI/MessageBox.png",
        "./resources/image/UI/options_backtogamebutton0.png",
        "./resources/image/UI/options_backtogamebutton_light.png",
        "./resources/image/Shovel.png",
		"./resources/image/PlantImage/PeaShooter.png",
		"./resources/image/PlantImage/SunFlower.png",
        "./resources/image/PlantImage/CherryBomb.png",
		"./resources/image/PlantImage/WallNut.png",
        "./resources/image/PlantImage/PotatoMine.png",
		"./resources/image/PlantImage/SnowPeaShooter.png",
        "./resources/image/PlantImage/Chomper.png",
        "./resources/image/PlantImage/Repeater.png",
        "./resources/image/PlantImage/PuffShroom.png",
        "./resources/image/PlantImage/SunShroom.png",
        "./resources/image/PlantImage/FumeShroom.png",
        "./resources/image/PlantImage/GraveBuster.png",
        "./resources/image/PlantImage/HypnoShroom.png",
        "./resources/image/PlantImage/ScaredyShroom.png",
        "./resources/image/PlantImage/IceShroom.png",
		"./resources/image/PlantImage/DoomShroom.png",
        "./resources/image/PlantImage/Squash.png",
        "./resources/image/PlantImage/Threepeater.png",
        "./resources/image/PlantImage/TangleKelp.png",
        "./resources/image/PlantImage/Jalapeno.png",
        "./resources/image/PlantImage/TorchWood.png",
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
        "./resources/sounds/UI/bleep.ogg",          // 划到主菜单按钮
        "./resources/sounds/UI/buttonclick.ogg",    // 按钮点击
        "./resources/sounds/UI/ceramic.ogg",       
        "./resources/sounds/UI/chooseplant1.ogg",   // 选卡见面点击植物
		"./resources/sounds/UI/chooseplant2.ogg",   // 选卡见面点击植物2
        "./resources/sounds/UI/gravebutton.ogg",    // 点击主菜单按钮
        "./resources/sounds/UI/mainmenu.ogg",       // 进主菜单的音乐
		"./resources/sounds/UI/pause.ogg",          // 暂停菜单
        "./resources/sounds/UI/shovel.ogg",         // 拿起铲子
		"./resources/sounds/collectsun.ogg",        // 收集阳光
        "./resources/sounds/UI/clickfailed.ogg",    // 没阳光点击卡槽   
        "./resources/sounds/Afterhugewave.ogg",
        "./resources/sounds/finalwave.ogg",
        "./resources/sounds/firstwave.ogg",
        "./resources/sounds/lawnmower.ogg",         // 小推车启动
        "./resources/sounds/lostgame.ogg",          // 游戏失败
        "./resources/sounds/pool_cleaner.ogg",      // 水上小推车启动
        "./resources/sounds/readysetplant.ogg",     // 开始前的声音
        "./resources/sounds/winmusic.ogg",          // 游戏胜利
    };

    std::vector<std::string> musicPaths = {
        "./resources/music/MainMenu.ogg",
    };

public:
    // 单例访问
    static ResourceManager& GetInstance();
    static void ReleaseInstance();

    // 统一加载方法
    bool LoadAllGameImages();
    bool LoadAllParticleTextures();
    bool LoadAllFonts();
    bool LoadAllSounds();
    bool LoadAllMusic();
    bool LoadAllReanimations();

    // 获取资源组
    const std::vector<std::string>& GetGameImagePaths() const;
    const std::vector<std::string>& GetParticleTexturePaths() const;
    const std::vector<std::string>& GetSoundPaths() const;
    const std::vector<std::string>& GetMusicPaths() const;
    const std::unordered_map<std::string, std::string>& GetAnimationPaths() const { return mReanimationPaths; }

    // 初始化
    void Initialize(SDL_Renderer* renderer);

    // 纹理管理
    SDL_Texture* LoadTexture(const std::string& path, const std::string& key = "");
    SDL_Texture* GetTexture(const std::string& key);
    void UnloadTexture(const std::string& key);

    std::shared_ptr<Reanimation> LoadReanimation(const std::string& key, const std::string& path);
    std::shared_ptr<Reanimation> GetReanimation(const std::string& key);
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

    ResourceManager(const ResourceManager&) = delete;
    ResourceManager& operator=(const ResourceManager&) = delete;
};

#endif