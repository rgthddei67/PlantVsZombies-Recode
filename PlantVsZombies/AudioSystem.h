#pragma once
#ifndef _AUDIOSYSTEM_H
#define _AUDIOSYSTEM_H

#include <SDL_mixer.h>
#include <string>

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

    // 音乐常量
    const std::string MUSIC_MENU = "music_MainMenu";
}

class AudioSystem
{
public:
    static bool Initialize();
    static void Shutdown();

    static void PlaySound(const std::string& key, int loops = 0);
    static void PlayMusic(const std::string& key, int loops = -1);
    static void StopMusic();
    static void SetVolume(int volume); // 0-128
    static void SetSoundVolume(const std::string& key, int volume);

    static bool IsAudioAvailable();
};

#endif