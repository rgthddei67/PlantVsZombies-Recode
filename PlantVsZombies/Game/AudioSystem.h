#pragma once
#ifndef _AUDIOSYSTEM_H
#define _AUDIOSYSTEM_H
#include <SDL_mixer.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>

//TODO:������Ч��Ҫ������
namespace AudioConstants
{
    // ��Ч����
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

    // ���ֳ���
    const std::string MUSIC_MENU = "music_MainMenu";
}

class AudioSystem
{
private:
    static float masterVolume;        // ������ 0.0 - 1.0
    static float soundVolume;         // ��Ч���� 0.0 - 1.0
    static float musicVolume;         // �������� 0.0 - 1.0
    static std::unordered_map<std::string, float> soundVolumes; // ������Ч����

public:
    // ��ʼ��
    static bool Initialize();
    static void Shutdown();

    // �������� - ������
    static void SetMasterVolume(float volume);
    static float GetMasterVolume();

    // �������� - ��Ч
    static void SetSoundVolume(float volume);
    static float GetSoundVolume();

    // �������� - ����
    static void SetMusicVolume(float volume);
    static float GetMusicVolume();

    // ������Ч��������
    static void SetSoundVolume(const std::string& soundKey, float volume);
    static float GetSoundVolume(const std::string& soundKey);

    // ��������
    static void PlaySound(const std::string& soundKey, int loops = 0);
    // ����ָ������ʱ������
    static void PlaySound(const std::string& soundKey, float volume, int loops = 0);
    // ����ָ������ʱ������������
    static void PlaySound(const std::string& soundKey, float volume, int loops, int channel);

    static void PlayMusic(const std::string& musicKey, int loops = -1);
    static void StopMusic();
    static void PauseMusic();
    static void ResumeMusic();

    // ���߷���
    static bool IsAudioAvailable();
    static void UpdateVolume(); // ������������

    // ����/������������
    static void SaveVolumeSettings();
    static void LoadVolumeSettings();
};

#endif