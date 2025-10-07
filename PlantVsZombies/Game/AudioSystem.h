#pragma once
#ifndef _AUDIOSYSTEM_H
#define _AUDIOSYSTEM_H
#include <SDL_mixer.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>

//TODO:新增音效还要改这里
namespace AudioConstants
{
    // 音效常量
    const std::string SOUND_BLEEP = "sound_bleep";
    const std::string SOUND_BUTTONCLICK = "sound_buttonclick";
    const std::string SOUND_CERAMIC = "sound_ceramic";
    const std::string SOUND_CHOOSEPLANT1 = "sound_chooseplant1";
    const std::string SOUND_CHOOSEPLANT2 = "sound_chooseplant2";
    const std::string SOUND_GRAVEBUTTON = "sound_gravebutton";
    const std::string SOUND_MAINMENU = "sound_mainmenu";
    const std::string SOUND_PAUSE = "sound_pause";
    const std::string SOUND_SHOVEL = "sound_shovel";
    const std::string SOUND_COLLECT_SUN = "sound_collectsun";

    // 音乐常量
    const std::string MUSIC_MENU = "music_MainMenu";
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