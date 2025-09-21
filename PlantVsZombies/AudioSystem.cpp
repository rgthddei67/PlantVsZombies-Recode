#include "AudioSystem.h"
#include "ResourceManager.h"
#include <iostream>

bool AudioSystem::Initialize()
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        std::cerr << "音频初始化失败: " << Mix_GetError() << std::endl;
        return false;
    }

    Mix_AllocateChannels(16);
    std::cout << "音频系统初始化成功" << std::endl;
    return true;
}

void AudioSystem::Shutdown()
{
    Mix_CloseAudio();
}

void AudioSystem::PlaySound(const std::string& key, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(key);
    if (sound)
    {
        Mix_PlayChannel(-1, sound, loops);
    }
}

void AudioSystem::PlayMusic(const std::string& key, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Music* music = ResourceManager::GetInstance().GetMusic(key);
    if (music)
    {
        Mix_PlayMusic(music, loops);
    }
}

void AudioSystem::StopMusic()
{
    if (IsAudioAvailable())
    {
        Mix_HaltMusic();
    }
}

void AudioSystem::SetVolume(int volume)
{
    if (IsAudioAvailable())
    {
        Mix_Volume(-1, volume);
        Mix_VolumeMusic(volume);
    }
}

void AudioSystem::SetSoundVolume(const std::string& key, int volume)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(key);
    if (sound)
    {
        Mix_VolumeChunk(sound, volume);
    }
}

bool AudioSystem::IsAudioAvailable()
{
    return Mix_QuerySpec(nullptr, nullptr, nullptr) != 0;
}