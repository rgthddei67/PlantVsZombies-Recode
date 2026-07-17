#include "../Game/AudioSystem.h"
#include "../Game/AdaptiveMusicPlayer.h"
#include "../ResourceManager.h"
#include "../Logger.h"
#include <algorithm>

namespace
{
	AdaptiveMusicTune GetAdaptiveTune(const std::string& musicKey)
	{
		if (musicKey == ResourceKeys::Music::MUSIC_DAY) return AdaptiveMusicTune::DAY;
		if (musicKey == ResourceKeys::Music::MUSIC_NIGHT) return AdaptiveMusicTune::NIGHT;
		if (musicKey == ResourceKeys::Music::MUSIC_POOL) return AdaptiveMusicTune::POOL;
		if (musicKey == ResourceKeys::Music::MUSIC_FOG) return AdaptiveMusicTune::FOG;
		if (musicKey == ResourceKeys::Music::MUSIC_ROOF) return AdaptiveMusicTune::ROOF;
		return AdaptiveMusicTune::NONE;
	}
}

float AudioSystem::masterVolume = 1.0f;
float AudioSystem::soundVolume = 0.5f;
float AudioSystem::musicVolume = 0.5f;
std::unordered_map<std::string, float> AudioSystem::soundVolumes;

bool AudioSystem::Initialize()
{
	if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 2048) < 0)
	{
		LOG_ERROR("AudioSystem") << "音频初始化失败: " << Mix_GetError();
		return false;
	}

	Mix_AllocateChannels(128);
	return true;
}

void AudioSystem::Shutdown() {
	AdaptiveMusicPlayer::GetInstance().Stop();
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
	}
}

// 播放音乐（带音量控制）
void AudioSystem::PlayMusic(const std::string& musicKey, int loops)
{
	if (!IsAudioAvailable()) return;

	auto& adaptiveMusic = AdaptiveMusicPlayer::GetInstance();
	const AdaptiveMusicTune tune = GetAdaptiveTune(musicKey);
	if (tune != AdaptiveMusicTune::NONE)
	{
		adaptiveMusic.SetVolume(masterVolume * musicVolume);
		if (adaptiveMusic.Play(tune)) return;
	}

	// 非关卡音乐和 MO3 加载失败都继续使用现有 OGG 播放路径。
	adaptiveMusic.Stop();

	Mix_Music* music = ResourceManager::GetInstance().GetMusic(musicKey);
	if (music)
	{
		// 计算最终音量：主音量 × 音乐音量
		int finalVolume = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);

		Mix_VolumeMusic(finalVolume);
		Mix_PlayMusic(music, loops);
	}
}

void AudioSystem::StopMusic()
{
	if (IsAudioAvailable())
	{
		AdaptiveMusicPlayer::GetInstance().Stop();
		Mix_HaltMusic();
	}
}

void AudioSystem::PauseMusic()
{
	if (IsAudioAvailable())
	{
		auto& adaptiveMusic = AdaptiveMusicPlayer::GetInstance();
		if (adaptiveMusic.IsPlaying()) adaptiveMusic.Pause(true);
		else Mix_PauseMusic();
	}
}

void AudioSystem::ResumeMusic()
{
	if (IsAudioAvailable())
	{
		auto& adaptiveMusic = AdaptiveMusicPlayer::GetInstance();
		if (adaptiveMusic.IsPlaying()) adaptiveMusic.Pause(false);
		else Mix_ResumeMusic();
	}
}

void AudioSystem::UpdateAdaptiveMusic(float deltaTime, int hostileZombieCount)
{
	AdaptiveMusicPlayer::GetInstance().Update(deltaTime, hostileZombieCount);
}

void AudioSystem::StartMusicBurst()
{
	AdaptiveMusicPlayer::GetInstance().StartBurst();
}

void AudioSystem::UpdateVolume()
{
	if (!IsAudioAvailable()) return;

	// 更新音乐音量
	int musicVol = static_cast<int>(MIX_MAX_VOLUME * masterVolume * musicVolume);
	Mix_VolumeMusic(musicVol);
	AdaptiveMusicPlayer::GetInstance().SetVolume(masterVolume * musicVolume);
}

bool AudioSystem::IsAudioAvailable()
{
	return Mix_QuerySpec(nullptr, nullptr, nullptr) != 0;
}
