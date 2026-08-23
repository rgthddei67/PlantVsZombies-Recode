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

	/** 在后台构建指定关卡的完整 tracker 分轨；重复请求同一 tune 不会重复工作。 */
	void Prepare(AdaptiveMusicTune tune);
	/** 接管已预构建的分轨；尚未完成时返回 false，由 AudioSystem 先播放 OGG。 */
	bool Play(AdaptiveMusicTune tune);
	void Stop();
	/** 停止播放并等待后台 worker 退出；只在 AudioSystem 关闭时调用。 */
	void Shutdown();
	void Pause(bool paused);
	void SetVolume(float volume);

	void Update(float deltaTime, int hostileZombieCount);
	void StartBurst();
	bool IsPlaying() const;
	AdaptiveMusicTune GetCurrentTune() const;
	AdaptiveMusicTune GetPreparedTune() const;
	bool DidLastPlayStartImmediately() const;
	int GetLastPreparationMilliseconds() const;
	int GetLastPlayHandoffMicroseconds() const;

	~AdaptiveMusicPlayer();

private:
	AdaptiveMusicPlayer();
	AdaptiveMusicPlayer(const AdaptiveMusicPlayer&) = delete;
	AdaptiveMusicPlayer& operator=(const AdaptiveMusicPlayer&) = delete;

	struct Impl;
	std::unique_ptr<Impl> mImpl;
};
