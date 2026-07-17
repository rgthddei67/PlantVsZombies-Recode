#pragma once

#include <memory>

enum class AdaptiveMusicTune
{
	NONE,
	DAY,
	NIGHT,
	POOL,
	FOG,
	ROOF
};

// 原版关卡音乐使用 MO3 tracker 的多通道动态混音：主旋律常开，僵尸压力升高时
// 在小节边界加入鼓组/踩镲。此类只负责 MO3 播放和 burst 状态；普通 OGG 仍由 AudioSystem 管理。
class AdaptiveMusicPlayer
{
public:
	static AdaptiveMusicPlayer& GetInstance();

	bool Play(AdaptiveMusicTune tune);
	void Stop();
	void Pause(bool paused);
	void SetVolume(float volume);

	void Update(float deltaTime, int hostileZombieCount);
	void StartBurst();
	bool IsPlaying() const;

	~AdaptiveMusicPlayer();

private:
	AdaptiveMusicPlayer();
	AdaptiveMusicPlayer(const AdaptiveMusicPlayer&) = delete;
	AdaptiveMusicPlayer& operator=(const AdaptiveMusicPlayer&) = delete;

	struct Impl;
	std::unique_ptr<Impl> mImpl;
};
