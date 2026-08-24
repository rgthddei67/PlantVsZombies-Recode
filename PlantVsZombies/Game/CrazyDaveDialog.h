#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

class Animator;
class Graphics;

/**
 * @brief 选卡演出前的疯狂戴夫闲聊模态框。
 * @details 对话只保存稳定关卡号；天气、迷雾等玩法状态仍由 Board 独立拥有。
 */
class CrazyDaveDialog {
public:
	enum class Phase {
		INACTIVE,
		ENTERING,
		TALKING,
		LEAVING,
	};

	using CompletionCallback = std::function<void()>;

	CrazyDaveDialog() = default;
	~CrazyDaveDialog();

	/** 查询关卡是否配置了戴夫闲聊。 */
	static bool SupportsLevel(int level);
	/** 返回 CrazyDave.reanim 实际引用的部件数，供资源闭环测试。 */
	static int GetRequiredTextureCount();
	/** 返回当前已按 reanim 资源键加载的部件数。 */
	static int GetLoadedRequiredTextureCount();

	/**
	 * @brief 开始指定关卡的闲聊；资源或配置不完整时返回 false，不阻塞正常开局。
	 * @param onCompleted 对话完整结束或主动跳过后的回调。
	 */
	bool Start(int level, CompletionCallback onCompleted);
	void Update();
	void Draw(Graphics* g);

	/** 推进一页；入场阶段第一次推进只会跳到首句，不会漏句。 */
	bool Advance();
	/** 立即结束整段闲聊；与完整看完一样记为已读。 */
	bool Skip();

	bool IsActive() const { return mPhase != Phase::INACTIVE; }
	Phase GetPhase() const { return mPhase; }
	const char* GetPhaseName() const;
	int GetLevel() const { return mLevel; }
	int GetMessageIndex() const { return mMessageIndex; }
	int GetMessageCount() const { return static_cast<int>(mMessages.size()); }
	const std::string& GetCurrentText() const;
	std::string GetCurrentTrackName() const;
	int GetRenderedQuadCount() const;
	bool HasRenderedGeometry() const;
	bool UsedInstanceRenderPath() const;

private:
	struct Message {
		std::string text;
		std::string talkTrack;
	};

	void BeginCurrentMessage();
	void BeginLeaving();
	void Finish();

	int mLevel = 0;
	int mMessageIndex = 0;
	Phase mPhase = Phase::INACTIVE;
	std::vector<Message> mMessages;
	std::shared_ptr<Animator> mAnimator;
	CompletionCallback mOnCompleted;
};
