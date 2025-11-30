#pragma once
#ifndef _AUDIOSYSTEM_H
#define _AUDIOSYSTEM_H
#ifdef PlaySound
#undef PlaySound
#endif
#include <SDL2/SDL_mixer.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>

//TODO:新增音效还要改这里
namespace AudioConstants
{
    // 音效常量
    const std::string SOUND_BLEEP = "SOUND_BLEEP";              // 碰到主菜单按钮
    const std::string SOUND_BUTTONCLICK = "SOUND_BUTTONCLICK";
    const std::string SOUND_CERAMIC = "SOUND_CERAMIC";
    const std::string SOUND_CHOOSEPLANT1 = "SOUND_CHOOSEPLANT1";
    const std::string SOUND_CHOOSEPLANT2 = "SOUND_CHOOSEPLANT2";
    const std::string SOUND_GRAVEBUTTON = "SOUND_GRAVEBUTTON";
    const std::string SOUND_MAINMENU = "SOUND_MAINMENU";
    const std::string SOUND_PAUSE = "SOUND_PAUSE";
    const std::string SOUND_SHOVEL = "SOUND_SHOVEL";
    const std::string SOUND_COLLECT_SUN = "SOUND_COLLECTSUN";
    const std::string SOUND_CLICK_FAILED = "SOUND_CLICKFAILED";
    const std::string SOUND_AFTER_HUGE_WAVE = "SOUND_AFTERHUGEWAVE";
    const std::string SOUND_FINAL_WAVE = "SOUND_FINALWAVE";
    const std::string SOUND_FIRST_WAVE = "SOUND_FIRSTWAVE";
    const std::string SOUND_LAWN_MOWER = "SOUND_LAWNMOWER";
    const std::string SOUND_LOST_GAME = "SOUND_LOSTGAME";
    const std::string SOUND_POOL_CLEANER = "SOUND_POOL_CLEANER";
    const std::string SOUND_READY_SET_PLANT = "SOUND_READYSETPLANT";
    const std::string SOUND_WIN_MUSIC = "SOUND_WINMUSIC";
    const std::string SOUND_CLICK_SEED = "SOUND_CLICKSEED";
    const std::string SOUND_DELETEPLANT = "SOUND_DELETEPLANT";
    const std::string SOUND_PLANT = "SOUND_PLANT";
    const std::string SOUND_PLANT_ONWATER = "SOUND_PLANT_ONWATER";

    // 音乐常量
    const std::string MUSIC_MENU = "MUSIC_MAINMENU";
}

class AudioSystem
{
private:
    static float masterVolume;        // 总音量 0.0 - 1.0
    static float soundVolume;         // 音效音量 0.0 - 1.0
    static float musicVolume;         // 音乐音量 0.0 - 1.0
    static std::unordered_map<std::string, float> soundVolumes; // 单独音效音量

public:
    // 初始化
    static bool Initialize();
    static void Shutdown();

    // 音量控制 - 总音量
    static void SetMasterVolume(float volume);
    static float GetMasterVolume();

    // 音量控制 - 音效
    static void SetSoundVolume(float volume);
    static float GetSoundVolume();

    // 音量控制 - 音乐
    static void SetMusicVolume(float volume);
    static float GetMusicVolume();

    // 单独音效音量控制
    static void SetSoundVolume(const std::string& soundKey, float volume);
    static float GetSoundVolume(const std::string& soundKey);

    // 播放音量
    static void PlaySound(const std::string& soundKey, int loops = 0);
    // 可以指定播放时的音量
    static void PlaySound(const std::string& soundKey, float volume, int loops = 0);
    // 可以指定播放时的音量和声道
    static void PlaySound(const std::string& soundKey, float volume, int loops, int channel);

    static void PlayMusic(const std::string& musicKey, int loops = -1);
    static void StopMusic();
    static void PauseMusic();
    static void ResumeMusic();

    // 工具方法
    static bool IsAudioAvailable();
    static void UpdateVolume(); // 更新所有音量

    // 保存/加载音量设置
    static void SaveVolumeSettings();
    static void LoadVolumeSettings();
};

#endif