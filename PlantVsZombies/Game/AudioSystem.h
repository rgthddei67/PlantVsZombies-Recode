#pragma once
#ifndef _AUDIOSYSTEM_H
#define _AUDIOSYSTEM_H
#ifdef PlaySound
#undef PlaySound
#endif
#include "../ResourceKeys.h"
#include <SDL2/SDL_mixer.h>
#include <string>
#include <algorithm>
#include <iostream>
#include <unordered_map>

class AudioSystem
{
private:
	struct LoopingSoundState
	{
		int channel = -1;
		float volume = 1.0f;
	};

	static float masterVolume;        // 总音量 0.0 - 1.0
	static float soundVolume;         // 音效音量 0.0 - 1.0
	static float musicVolume;         // 音乐音量 0.0 - 1.0
	static std::unordered_map<std::string, float> soundVolumes; // 单独音效音量
	static std::unordered_map<std::string, LoopingSoundState> loopingSounds;

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

	/** 按资源键启动唯一的循环环境音；重复调用只更新该循环的音量。 */
	static void PlayLoopingSound(const std::string& soundKey, float volume = 1.0f);
	/** 停止指定资源键启动的循环环境音。 */
	static void StopLoopingSound(const std::string& soundKey);
	/** 查询指定循环环境音当前是否仍占用并播放其 SDL_mixer 声道。 */
	static bool IsLoopingSoundPlaying(const std::string& soundKey);

	static void PlayMusic(const std::string& musicKey, int loops = -1);
	static void StopMusic();
	static void PauseMusic();
	static void ResumeMusic();
	static void UpdateAdaptiveMusic(float deltaTime, int hostileZombieCount);
	static void StartMusicBurst();

	// 工具方法
	static bool IsAudioAvailable();
	static void UpdateVolume(); // 更新所有音量
};

#endif
