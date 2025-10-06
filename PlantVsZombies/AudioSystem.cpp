#include "AudioSystem.h"
#include "ResourceManager.h"

#include <fstream>
#include <algorithm>

// ��̬��Ա��������
float AudioSystem::masterVolume = 1.0f;
float AudioSystem::soundVolume = 1.0f;
float AudioSystem::musicVolume = 1.0f;
std::unordered_map<std::string, float> AudioSystem::soundVolumes;

bool AudioSystem::Initialize()
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
    {
        std::cerr << "��Ƶ��ʼ��ʧ��: " << Mix_GetError() << std::endl;
        return false;
    }

    Mix_AllocateChannels(16);
    /*
    // ���ر������������
    LoadVolumeSettings();

    std::cout << "��Ƶϵͳ��ʼ���ɹ�" << std::endl;
    std::cout << "������: " << masterVolume
        << ", ��Ч����: " << soundVolume
        << ", ��������: " << musicVolume << std::endl;
    */
    return true;
}

void AudioSystem::Shutdown() {
#ifdef _DEBUG
    std::cout << "=== AudioSystem Shutdown ===" << std::endl;
    std::cout << "����ǰ soundVolumes ��С: " << soundVolumes.size() << std::endl;
#endif
    // ��ϸ�������
#ifdef _DEBUG
    for (const auto& pair : soundVolumes) {
        std::cout << "  ��������: " << pair.first << " -> " << pair.second << std::endl;
    }
#endif
    soundVolumes.clear();
#ifdef _DEBUG
    std::cout << "����� soundVolumes ��С: " << soundVolumes.size() << std::endl;
#endif
    Mix_HaltChannel(-1);
    Mix_HaltMusic();
    Mix_CloseAudio();
#ifdef _DEBUG
    std::cout << "=== AudioSystem �ر���� ===" << std::endl;
#endif
}

// ����������
void AudioSystem::SetMasterVolume(float volume)
{
    masterVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetMasterVolume()
{
    return masterVolume;
}

// ��Ч��������
void AudioSystem::SetSoundVolume(float volume)
{
    soundVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetSoundVolume()
{
    return soundVolume;
}

// ������������
void AudioSystem::SetMusicVolume(float volume)
{
    musicVolume = std::clamp(volume, 0.0f, 1.0f);
    UpdateVolume();
}

float AudioSystem::GetMusicVolume()
{
    return musicVolume;
}

// ������Ч��������
void AudioSystem::SetSoundVolume(const std::string& soundKey, float volume)
{
    soundVolumes[soundKey] = std::clamp(volume, 0.0f, 1.0f);

    // �������¸���Ч��������������ڲ��ţ�
    // ���������չΪ�������ڲ��ŵ���Ч
}

float AudioSystem::GetSoundVolume(const std::string& soundKey)
{
    auto it = soundVolumes.find(soundKey);
    if (it != soundVolumes.end())
    {
        return it->second;
    }
    return 1.0f; // Ĭ������
}

// ������Ч
void AudioSystem::PlaySound(const std::string& soundKey, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(soundKey);
    if (sound)
    {
        // ʹ��Ԥ�����������
        float individualVolume = GetSoundVolume(soundKey);
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * soundVolume * individualVolume);

        Mix_VolumeChunk(sound, finalVolume);
        Mix_PlayChannel(-1, sound, loops);
    }
}

// ָ����������
void AudioSystem::PlaySound(const std::string& soundKey, float volume, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Chunk* sound = ResourceManager::GetInstance().GetSound(soundKey);
    if (sound)
    {
        // ʹ��ָ��������������Ԥ������
        volume = std::clamp(volume, 0.0f, 1.0f);
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * soundVolume * volume);

        Mix_VolumeChunk(sound, finalVolume);
        Mix_PlayChannel(-1, sound, loops);

        //std::cout << "������Ч: " << soundKey << " ָ������: " << volume << " ��������: " << finalVolume << std::endl;
    }
}

// ָ����������������
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

        //std::cout << "������Ч: " << soundKey << " ����: " << channel << " ����: " << finalVolume << std::endl;
    }
}

// �������֣����������ƣ�
void AudioSystem::PlayMusic(const std::string& musicKey, int loops)
{
    if (!IsAudioAvailable()) return;

    Mix_Music* music = ResourceManager::GetInstance().GetMusic(musicKey);
    if (music)
    {
        // �������������������� �� ��������
        int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);

        Mix_VolumeMusic(finalVolume);
        Mix_PlayMusic(music, loops);

        //std::cout << "��������: " << musicKey << " ����: " << finalVolume << std::endl;
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

    // ������������
    int musicVol = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);
    Mix_VolumeMusic(musicVol);

    // ����������Ч�������²��ŵ���Ч���Զ�Ӧ����������
#ifdef _DEBUG
    std::cout << "�������� - ������: " << masterVolume
        << ", ��Ч: " << soundVolume
        << ", ����: " << musicVolume
        << ", ������������: " << musicVol << std::endl;
#endif
}

bool AudioSystem::IsAudioAvailable()
{
    return Mix_QuerySpec(nullptr, nullptr, nullptr) != 0;
}

// ������������
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
        std::cout << "���������ѱ���" << std::endl;
#endif
    }
}

// ������������
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
        std::cout << "���������Ѽ���" << std::endl;
#endif
    }
}