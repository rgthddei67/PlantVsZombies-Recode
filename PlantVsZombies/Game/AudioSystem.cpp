#include "../Game/AudioSystem.h"
#include "../ResourceManager.h"
#include <fstream>
#include <algorithm>

// 静态成员变量定义
float AudioSystem::masterVolume = 1.0f;
float AudioSystem::soundVolume = 1.0f;
float AudioSystem::musicVolume = 1.0f;
std::unordered_map<std::string, float> AudioSystem::soundVolumes;

bool AudioSystem::Initialize()
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        std::cerr << "音频初始化失败: " << Mix_GetError() << std::endl;
        return false;
    }

    Mix_AllocateChannels(16);
    /*
    // 加载保存的音量设置
    LoadVolumeSettings();

    std::cout << "音频系统初始化成功" << std::endl;
    std::cout << "主音量: " << masterVolume
        << ", 音效音量: " << soundVolume
        << ", 音乐音量: " << musicVolume << std::endl;
    */
    return true;
}

void AudioSystem::Shutdown() {
    Mix_HaltChannel(-1);
    Mix_HaltMusic();

    int maxWait = 100;
    while (Mix_Playing(-1) && maxWait-- > 0) {
        SDL_Delay(1);
    }

    soundVolumes.clear();
    Mix_CloseAudio();
}

// 总音量控制
void AudioSystem::SetMasterVolume(float volume)
{
    masterVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetMasterVolume()
{
    return masterVolume;
}

// 音效音量控制
void AudioSystem::SetSoundVolume(float volume)
{
    soundVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetSoundVolume()
{
    return soundVolume;
}

// 音乐音量控制
void AudioSystem::SetMusicVolume(float volume)
{
    musicVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetMusicVolume()
{
    return musicVolume;
}

// 单独音效音量控制
void AudioSystem::SetSoundVolume(const std::string& soundKey, float volume)
{
    soundVolumes[soundKey] = std::clamp(volume, 0.0f, 1.0f);

    // 立即更新该音效的音量（如果正在播放）
    // 这里可以扩展为更新正在播放的音效
}

float AudioSystem::GetSoundVolume(const std::string& soundKey)
{
    auto it = soundVolumes.find(soundKey);
    if (it != soundVolumes.end())
    {
        return it->second;
    }
    return 1.0f; // 默认音量
}

// 播放音效
void AudioSystem::PlaySound(const std::string& soundKey, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(soundKey);
    if (sound)
    {
        // 使用预设的音量设置
        float individualVolume = GetSoundVolume(soundKey);
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * soundVolume * individualVolume);

        Mix_VolumeChunk(sound, finalVolume);
        Mix_PlayChannel(-1, sound, loops);
    }
}

// 指定播放音量
void AudioSystem::PlaySound(const std::string& soundKey, float volume, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(soundKey);
    if (sound)
    {
        // 使用指定的音量，覆盖预设设置
        volume = std::clamp(volume, 0.0f, 1.0f);
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * soundVolume * volume);

        Mix_VolumeChunk(sound, finalVolume);
        Mix_PlayChannel(-1, sound, loops);

        //std::cout << "播放音效: " << soundKey << " 指定音量: " << volume << " 最终音量: " << finalVolume << std::endl;
    }
}

// 指定播放音量和声道
void AudioSystem::PlaySound(const std::string& soundKey, float volume, int loops, int channel)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(soundKey);
    if (sound)
    {
        volume = std::clamp(volume, 0.0f, 1.0f);
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * soundVolume * volume);

        Mix_VolumeChunk(sound, finalVolume);

        if (channel >= 0) {
            Mix_PlayChannel(channel, sound, loops);
        }
        else {
            Mix_PlayChannel(-1, sound, loops);
        }

        //std::cout << "播放音效: " << soundKey << " 声道: " << channel << " 音量: " << finalVolume << std::endl;
    }
}

// 播放音乐（带音量控制）
void AudioSystem::PlayMusic(const std::string& musicKey, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Music* music = ResourceManager::GetInstance().GetMusic(musicKey);
    if (music)
    {
        // 计算最终音量：主音量 × 音乐音量
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);

        Mix_VolumeMusic(finalVolume);
        Mix_PlayMusic(music, loops);

        //std::cout << "播放音乐: " << musicKey << " 音量: " << finalVolume << std::endl;
    }
}

void AudioSystem::StopMusic()
{
    if (IsAudioAvailable())
    {
        Mix_HaltMusic();
    }
}

void AudioSystem::PauseMusic()
{
    if (IsAudioAvailable())
    {
        Mix_PauseMusic();
    }
}

void AudioSystem::ResumeMusic()
{
    if (IsAudioAvailable())
    {
        Mix_ResumeMusic();
    }
}

void AudioSystem::UpdateVolume()
{
    if (!IsAudioAvailable()) return;

    // 更新音乐音量
    int musicVol = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);
    Mix_VolumeMusic(musicVol);

    // 更新所有音效音量（新播放的音效会自动应用新音量）
#ifdef _DEBUG
    std::cout << "音量更新 - 主音量: " << masterVolume
        << ", 音效: " << soundVolume
        << ", 音乐: " << musicVolume
        << ", 最终音乐音量: " << musicVol << std::endl;
#endif
}

bool AudioSystem::IsAudioAvailable()
{
    return Mix_QuerySpec(nullptr, nullptr, nullptr) != 0;
}

// 保存音量设置
void AudioSystem::SaveVolumeSettings()
{
    std::ofstream file("volume_settings.cfg");
    if (file.is_open())
    {
        file << masterVolume << std::endl;
        file << soundVolume << std::endl;
        file << musicVolume << std::endl;
        file.close();
#ifdef _DEBUG
        std::cout << "音量设置已保存" << std::endl;
#endif
    }
}

// 加载音量设置
void AudioSystem::LoadVolumeSettings()
{
    std::ifstream file("volume_settings.cfg");
    if (file.is_open())
    {
        file >> masterVolume;
        file >> soundVolume;
        file >> musicVolume;
        file.close();
#ifdef _DEBUG
        std::cout << "音量设置已加载" << std::endl;
#endif
    }
}