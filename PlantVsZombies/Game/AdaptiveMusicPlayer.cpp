#include "AdaptiveMusicPlayer.h"

#include "../Logger.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <libopenmpt/libopenmpt_ext.hpp>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{
	constexpr float BURST_FADE_IN_SECONDS = 4.0f;
	constexpr float BURST_QUEUE_DRUMS_AFTER_SECONDS = 3.0f;
	constexpr float BURST_MIN_SECONDS = 8.0f;
	constexpr float BURST_FADE_OUT_SECONDS = 8.0f;
	constexpr float NIGHT_BURST_FINISH_SECONDS = 11.0f;
	constexpr float DRUMS_FADE_OUT_SECONDS = 0.5f;
	constexpr float NIGHT_DRUMS_FADE_OUT_SECONDS = 8.0f;

	constexpr int DAY_START_ORDER = 0;
	constexpr int NIGHT_MAIN_START_ORDER = 0x30;
	constexpr int NIGHT_DRUMS_START_ORDER = 0x5C;
	constexpr int POOL_START_ORDER = 0x5E;
	constexpr int FOG_START_ORDER = 0x7D;
	constexpr int ROOF_START_ORDER = 0xB8;

	constexpr int NIGHT_DRUMS_EVEN_ORDER = 76;
	constexpr int NIGHT_DRUMS_ODD_ORDER = 77;

	float Clamp01(float value)
	{
		return std::clamp(value, 0.0f, 1.0f);
	}

	std::vector<std::uint8_t> ReadBinaryFile(const std::vector<std::string>& candidates,
		std::string& loadedPath)
	{
		for (const auto& path : candidates)
		{
			std::ifstream stream(path, std::ios::binary | std::ios::ate);
			if (!stream) continue;

			const std::streamsize size = stream.tellg();
			if (size <= 0) continue;
			stream.seekg(0, std::ios::beg);

			std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
			if (!stream.read(reinterpret_cast<char*>(data.data()), size)) continue;

			loadedPath = path;
			return data;
		}
		return {};
	}
}

struct AdaptiveMusicPlayer::Impl
{
	enum class LayerRole
	{
		MAIN,
		DRUMS,
		HIHATS
	};

	enum class BurstState
	{
		OFF,
		STARTING,
		ON,
		FINISHING
	};

	enum class DrumsState
	{
		OFF,
		ON_QUEUED,
		ON,
		OFF_QUEUED,
		FADING
	};

	struct ChannelRange
	{
		int first;
		int last;
	};

	struct Layer
	{
		LayerRole role = LayerRole::MAIN;
		std::shared_ptr<std::ostringstream> log;
		std::unique_ptr<openmpt::module_ext> module;
		std::vector<float> renderBuffer;
	};

	struct Playback
	{
		AdaptiveMusicTune tune = AdaptiveMusicTune::NONE;
		std::vector<std::unique_ptr<Layer>> layers;
		Layer* main = nullptr;
		Layer* drums = nullptr;
		Layer* hihats = nullptr;
		std::vector<float> mixedBuffer;
	};

	std::mutex playbackMutex;
	std::unique_ptr<Playback> playback;
	std::vector<std::uint8_t> mainMusicData;
	std::vector<std::uint8_t> hihatsData;

	std::atomic<bool> playing{ false };
	std::atomic<bool> paused{ false };
	std::atomic<float> musicVolume{ 1.0f };
	std::atomic<float> mainVolume{ 1.0f };
	std::atomic<float> drumsVolume{ 0.0f };
	std::atomic<float> hihatsVolume{ 0.0f };
	std::atomic<int> mainOrder{ -1 };
	std::atomic<int> pendingDrumsJumpOrder{ -1 };

	AdaptiveMusicTune tune = AdaptiveMusicTune::NONE;
	BurstState burstState = BurstState::OFF;
	DrumsState drumsState = DrumsState::OFF;
	float burstTimer = 0.0f;
	float drumsTimer = 0.0f;
	int queuedDrumsOrder = -1;
	bool drumsQueuedDuringStart = false;
	int sampleRate = 44100;
	SDL_AudioFormat outputFormat = AUDIO_S16SYS;
	int outputChannels = 2;

	bool IsNightScheme() const
	{
		return tune == AdaptiveMusicTune::NIGHT;
	}

	bool EnsureAssetsLoaded()
	{
		if (!mainMusicData.empty() && !hihatsData.empty()) return true;

		std::string mainPath;
		std::string hihatsPath;
		mainMusicData = ReadBinaryFile({
			"./resources/music/mainmusic.mo3",
			"./resources/sounds/mainmusic.mo3"
		}, mainPath);
		hihatsData = ReadBinaryFile({
			"./resources/music/mainmusic_hihats.mo3",
			"./resources/sounds/mainmusic_hihats.mo3"
		}, hihatsPath);

		if (mainMusicData.empty() || hihatsData.empty())
		{
			LOG_WARN("AdaptiveMusic")
				<< "MO3 素材缺失，将回退到 OGG。需要 resources/music/mainmusic.mo3 和 mainmusic_hihats.mo3";
			mainMusicData.clear();
			hihatsData.clear();
			return false;
		}

		LOG_INFO("AdaptiveMusic") << "已加载 MO3: " << mainPath << " / " << hihatsPath;
		return true;
	}

	std::unique_ptr<Layer> CreateLayer(LayerRole role,
		const std::vector<std::uint8_t>& data,
		int startOrder,
		const std::vector<ChannelRange>& audibleRanges)
	{
		auto layer = std::make_unique<Layer>();
		layer->role = role;
		layer->log = std::make_shared<std::ostringstream>();
		layer->module = std::make_unique<openmpt::module_ext>(data, *layer->log);
		layer->module->set_repeat_count(-1);

		auto* interactive = static_cast<openmpt::ext::interactive*>(
			layer->module->get_interface(openmpt::ext::interactive_id));
		if (!interactive)
		{
			throw std::runtime_error("libopenmpt interactive interface unavailable");
		}

		const int channelCount = layer->module->get_num_channels();
		for (int channel = 0; channel < channelCount; ++channel)
		{
			bool audible = false;
			for (const auto& range : audibleRanges)
			{
				if (channel >= range.first && channel <= range.last)
				{
					audible = true;
					break;
				}
			}
			interactive->set_channel_mute_status(channel, !audible);
		}

		layer->module->set_position_order_row(startOrder, 0);
		return layer;
	}

	Layer* AddLayer(Playback& target, std::unique_ptr<Layer> layer)
	{
		Layer* raw = layer.get();
		target.layers.push_back(std::move(layer));
		return raw;
	}

	std::unique_ptr<Playback> BuildPlayback(AdaptiveMusicTune newTune)
	{
		auto target = std::make_unique<Playback>();
		target->tune = newTune;

		int startOrder = DAY_START_ORDER;
		std::vector<ChannelRange> mainRanges;
		std::vector<ChannelRange> drumsRanges;
		std::vector<ChannelRange> hihatsRanges;

		switch (newTune)
		{
		case AdaptiveMusicTune::DAY:
			startOrder = DAY_START_ORDER;
			mainRanges = { { 0, 23 } };
			drumsRanges = { { 24, 26 } };
			hihatsRanges = { { 27, 27 } };
			break;
		case AdaptiveMusicTune::POOL:
			startOrder = POOL_START_ORDER;
			mainRanges = { { 0, 17 } };
			drumsRanges = { { 25, 28 } };
			hihatsRanges = { { 18, 24 }, { 29, 29 } };
			break;
		case AdaptiveMusicTune::FOG:
			startOrder = FOG_START_ORDER;
			mainRanges = { { 0, 15 } };
			drumsRanges = { { 16, 22 } };
			hihatsRanges = { { 23, 23 } };
			break;
		case AdaptiveMusicTune::ROOF:
			startOrder = ROOF_START_ORDER;
			mainRanges = { { 0, 17 } };
			drumsRanges = { { 18, 20 } };
			hihatsRanges = { { 21, 21 } };
			break;
		case AdaptiveMusicTune::NIGHT:
			mainRanges = { { 0, 127 } };
			drumsRanges = { { 0, 127 } };
			target->main = AddLayer(*target,
				CreateLayer(LayerRole::MAIN, mainMusicData, NIGHT_MAIN_START_ORDER, mainRanges));
			target->drums = AddLayer(*target,
				CreateLayer(LayerRole::DRUMS, mainMusicData, NIGHT_DRUMS_START_ORDER, drumsRanges));
			return target;
		default:
			return nullptr;
		}

		target->main = AddLayer(*target,
			CreateLayer(LayerRole::MAIN, mainMusicData, startOrder, mainRanges));
		target->drums = AddLayer(*target,
			CreateLayer(LayerRole::DRUMS, mainMusicData, startOrder, drumsRanges));
		target->hihats = AddLayer(*target,
			CreateLayer(LayerRole::HIHATS, hihatsData, startOrder, hihatsRanges));
		return target;
	}

	static void SDLCALL MusicCallback(void* userData, Uint8* stream, int length)
	{
		static_cast<Impl*>(userData)->Render(stream, length);
	}

	void Render(Uint8* stream, int length)
	{
		std::memset(stream, 0, static_cast<std::size_t>(length));
		if (!playing.load(std::memory_order_relaxed) || paused.load(std::memory_order_relaxed)) return;
		if (outputFormat != AUDIO_S16SYS || outputChannels != 2) return;

		std::lock_guard<std::mutex> lock(playbackMutex);
		if (!playback) return;

		const std::size_t frameCount = static_cast<std::size_t>(length) /
			(sizeof(std::int16_t) * static_cast<std::size_t>(outputChannels));
		if (frameCount == 0) return;

		if (playback->drums)
		{
			const int jumpOrder = pendingDrumsJumpOrder.exchange(-1, std::memory_order_relaxed);
			if (jumpOrder >= 0)
			{
				playback->drums->module->set_position_order_row(jumpOrder, 0);
			}
		}

		playback->mixedBuffer.assign(frameCount * 2, 0.0f);
		for (auto& layer : playback->layers)
		{
			float layerVolume = 0.0f;
			switch (layer->role)
			{
			case LayerRole::MAIN:
				layerVolume = mainVolume.load(std::memory_order_relaxed);
				break;
			case LayerRole::DRUMS:
				layerVolume = drumsVolume.load(std::memory_order_relaxed);
				break;
			case LayerRole::HIHATS:
				layerVolume = hihatsVolume.load(std::memory_order_relaxed);
				break;
			}

			layer->renderBuffer.assign(frameCount * 2, 0.0f);
			const std::size_t rendered = layer->module->read_interleaved_stereo(
				sampleRate, frameCount, layer->renderBuffer.data());
			const std::size_t sampleCount = rendered * 2;
			for (std::size_t sample = 0; sample < sampleCount; ++sample)
			{
				playback->mixedBuffer[sample] += layer->renderBuffer[sample] * layerVolume;
			}
		}

		if (playback->main)
		{
			mainOrder.store(playback->main->module->get_current_order(), std::memory_order_relaxed);
		}

		const float overallVolume = Clamp01(musicVolume.load(std::memory_order_relaxed));
		auto* output = reinterpret_cast<std::int16_t*>(stream);
		for (std::size_t sample = 0; sample < playback->mixedBuffer.size(); ++sample)
		{
			const float scaled = std::clamp(
				playback->mixedBuffer[sample] * overallVolume, -1.0f, 1.0f);
			output[sample] = static_cast<std::int16_t>(std::lround(scaled * 32767.0f));
		}
	}

	void ResetBurstState()
	{
		burstState = BurstState::OFF;
		drumsState = DrumsState::OFF;
		burstTimer = 0.0f;
		drumsTimer = 0.0f;
		queuedDrumsOrder = -1;
		drumsQueuedDuringStart = false;
		mainVolume.store(1.0f, std::memory_order_relaxed);
		drumsVolume.store(0.0f, std::memory_order_relaxed);
		hihatsVolume.store(0.0f, std::memory_order_relaxed);
		mainOrder.store(-1, std::memory_order_relaxed);
		pendingDrumsJumpOrder.store(-1, std::memory_order_relaxed);
	}

	void QueueDrums(DrumsState queuedState)
	{
		drumsState = queuedState;
		queuedDrumsOrder = mainOrder.load(std::memory_order_relaxed);
	}

	void UpdateDrums(float deltaTime)
	{
		const int currentOrder = mainOrder.load(std::memory_order_relaxed);
		if (drumsState == DrumsState::ON_QUEUED && currentOrder != queuedDrumsOrder)
		{
			drumsState = DrumsState::ON;
			drumsVolume.store(1.0f, std::memory_order_relaxed);
			if (IsNightScheme() && currentOrder >= 0)
			{
				pendingDrumsJumpOrder.store(
					(currentOrder % 2 == 0) ? NIGHT_DRUMS_EVEN_ORDER : NIGHT_DRUMS_ODD_ORDER,
					std::memory_order_relaxed);
			}
		}
		else if (drumsState == DrumsState::OFF_QUEUED && currentOrder != queuedDrumsOrder)
		{
			drumsState = DrumsState::FADING;
			drumsTimer = DRUMS_FADE_OUT_SECONDS;
		}

		if (drumsState == DrumsState::FADING)
		{
			drumsTimer = std::max(0.0f, drumsTimer - deltaTime);
			const float duration = IsNightScheme()
				? NIGHT_DRUMS_FADE_OUT_SECONDS
				: DRUMS_FADE_OUT_SECONDS;
			drumsVolume.store(Clamp01(drumsTimer / duration), std::memory_order_relaxed);
			if (drumsTimer <= 0.0f)
			{
				drumsState = DrumsState::OFF;
				drumsVolume.store(0.0f, std::memory_order_relaxed);
			}
		}
	}

	void BeginBurst()
	{
		if (burstState != BurstState::OFF) return;
		burstState = BurstState::STARTING;
		burstTimer = BURST_FADE_IN_SECONDS;
		drumsQueuedDuringStart = false;
		if (IsNightScheme())
		{
			QueueDrums(DrumsState::ON_QUEUED);
			drumsQueuedDuringStart = true;
		}
	}

	void BeginFinishing()
	{
		burstState = BurstState::FINISHING;
		if (IsNightScheme())
		{
			burstTimer = NIGHT_BURST_FINISH_SECONDS;
			drumsState = DrumsState::FADING;
			drumsTimer = NIGHT_DRUMS_FADE_OUT_SECONDS;
		}
		else
		{
			burstTimer = BURST_FADE_OUT_SECONDS;
			QueueDrums(DrumsState::OFF_QUEUED);
		}
	}

	void UpdateBurst(float deltaTime, int hostileZombieCount)
	{
		if (!playing.load(std::memory_order_relaxed) || deltaTime <= 0.0f) return;

		UpdateDrums(deltaTime);
		switch (burstState)
		{
		case BurstState::OFF:
			if (hostileZombieCount >= 10) BeginBurst();
			break;

		case BurstState::STARTING:
			if (IsNightScheme())
			{
				if (drumsState == DrumsState::ON_QUEUED)
				{
					burstTimer = BURST_FADE_IN_SECONDS;
					mainVolume.store(1.0f, std::memory_order_relaxed);
					break;
				}

				burstTimer = std::max(0.0f, burstTimer - deltaTime);
				mainVolume.store(Clamp01(burstTimer / BURST_FADE_IN_SECONDS), std::memory_order_relaxed);
			}
			else
			{
				burstTimer = std::max(0.0f, burstTimer - deltaTime);
				const float elapsed = BURST_FADE_IN_SECONDS - burstTimer;
				hihatsVolume.store(Clamp01(elapsed / BURST_FADE_IN_SECONDS), std::memory_order_relaxed);
				if (!drumsQueuedDuringStart && elapsed >= BURST_QUEUE_DRUMS_AFTER_SECONDS)
				{
					QueueDrums(DrumsState::ON_QUEUED);
					drumsQueuedDuringStart = true;
				}
			}

			if (burstTimer <= 0.0f)
			{
				burstState = BurstState::ON;
				burstTimer = BURST_MIN_SECONDS;
				if (IsNightScheme()) mainVolume.store(0.0f, std::memory_order_relaxed);
				else hihatsVolume.store(1.0f, std::memory_order_relaxed);
			}
			break;

		case BurstState::ON:
			burstTimer = std::max(0.0f, burstTimer - deltaTime);
			if (IsNightScheme()) mainVolume.store(0.0f, std::memory_order_relaxed);
			else hihatsVolume.store(1.0f, std::memory_order_relaxed);
			if (drumsState == DrumsState::ON) drumsVolume.store(1.0f, std::memory_order_relaxed);
			if (burstTimer <= 0.0f && hostileZombieCount < 4) BeginFinishing();
			break;

		case BurstState::FINISHING:
			burstTimer = std::max(0.0f, burstTimer - deltaTime);
			if (IsNightScheme())
			{
				const float fadeIn = Clamp01((BURST_FADE_IN_SECONDS - burstTimer) / BURST_FADE_IN_SECONDS);
				mainVolume.store(fadeIn, std::memory_order_relaxed);
			}
			else
			{
				hihatsVolume.store(Clamp01(burstTimer / BURST_FADE_OUT_SECONDS), std::memory_order_relaxed);
			}

			if (burstTimer <= 0.0f && drumsState == DrumsState::OFF)
			{
				burstState = BurstState::OFF;
				mainVolume.store(1.0f, std::memory_order_relaxed);
				drumsVolume.store(0.0f, std::memory_order_relaxed);
				hihatsVolume.store(0.0f, std::memory_order_relaxed);
			}
			break;
		}
	}
};

AdaptiveMusicPlayer& AdaptiveMusicPlayer::GetInstance()
{
	static AdaptiveMusicPlayer instance;
	return instance;
}

AdaptiveMusicPlayer::AdaptiveMusicPlayer()
	: mImpl(std::make_unique<Impl>())
{
}

AdaptiveMusicPlayer::~AdaptiveMusicPlayer()
{
	Stop();
}

bool AdaptiveMusicPlayer::Play(AdaptiveMusicTune tune)
{
	if (tune == AdaptiveMusicTune::NONE || !mImpl->EnsureAssetsLoaded()) return false;

	int frequency = 0;
	SDL_AudioFormat format = 0;
	int channels = 0;
	if (!Mix_QuerySpec(&frequency, &format, &channels)) return false;
	if (format != AUDIO_S16SYS || channels != 2)
	{
		LOG_ERROR("AdaptiveMusic") << "不支持当前 SDL 混音格式，MO3 需要 S16 双声道";
		return false;
	}

	try
	{
		auto newPlayback = mImpl->BuildPlayback(tune);
		if (!newPlayback) return false;

		Mix_HaltMusic();
		{
			std::lock_guard<std::mutex> lock(mImpl->playbackMutex);
			mImpl->playback = std::move(newPlayback);
		}

		mImpl->sampleRate = frequency;
		mImpl->outputFormat = format;
		mImpl->outputChannels = channels;
		mImpl->tune = tune;
		mImpl->ResetBurstState();
		mImpl->paused.store(false, std::memory_order_relaxed);
		mImpl->playing.store(true, std::memory_order_relaxed);
		Mix_HookMusic(&Impl::MusicCallback, mImpl.get());

		LOG_INFO("AdaptiveMusic") << "MO3 动态音乐已启动，tune=" << static_cast<int>(tune);
		return true;
	}
	catch (const std::exception& exception)
	{
		LOG_ERROR("AdaptiveMusic") << "MO3 加载失败: " << exception.what();
		Stop();
		return false;
	}
}

void AdaptiveMusicPlayer::Stop()
{
	if (!mImpl) return;
	const bool wasPlaying = mImpl->playing.exchange(false, std::memory_order_relaxed);
	if (wasPlaying)
	{
		Mix_HookMusic(nullptr, nullptr);
	}
	{
		std::lock_guard<std::mutex> lock(mImpl->playbackMutex);
		mImpl->playback.reset();
	}
	mImpl->paused.store(false, std::memory_order_relaxed);
	mImpl->tune = AdaptiveMusicTune::NONE;
	mImpl->ResetBurstState();
}

void AdaptiveMusicPlayer::Pause(bool paused)
{
	mImpl->paused.store(paused, std::memory_order_relaxed);
}

void AdaptiveMusicPlayer::SetVolume(float volume)
{
	mImpl->musicVolume.store(Clamp01(volume), std::memory_order_relaxed);
}

void AdaptiveMusicPlayer::Update(float deltaTime, int hostileZombieCount)
{
	mImpl->UpdateBurst(deltaTime, hostileZombieCount);
}

void AdaptiveMusicPlayer::StartBurst()
{
	if (mImpl->playing.load(std::memory_order_relaxed)) mImpl->BeginBurst();
}

bool AdaptiveMusicPlayer::IsPlaying() const
{
	return mImpl->playing.load(std::memory_order_relaxed);
}
