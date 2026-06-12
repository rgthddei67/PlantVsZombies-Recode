#include "TestDriver.h"
#include "../../GameAPP.h"
#include "../../DeltaTime.h"
#include "../../Logger.h"
#include "../SceneManager.h"
#include <filesystem>

TestDriver& TestDriver::GetInstance() {
	static TestDriver instance;
	return instance;
}

bool TestDriver::LoadScript(const std::string& path) {
	std::ifstream f(path);
	if (!f) {
		LOG_ERROR("AutoTest") << "无法打开脚本: " << path;
		return false;
	}
	nlohmann::json j;
	try {
		f >> j;
	}
	catch (const std::exception& e) {
		LOG_ERROR("AutoTest") << "脚本 JSON 解析失败: " << e.what();
		return false;
	}
	if (!j.contains("commands") || !j["commands"].is_array() || j["commands"].empty()) {
		LOG_ERROR("AutoTest") << "脚本缺少非空 commands 数组";
		return false;
	}
	for (const auto& c : j["commands"]) mCommands.push_back(c);

	std::error_code ec;
	mOutDir = (std::filesystem::path("./autotest/out") /
		std::filesystem::path(path).stem()).string();
	std::filesystem::create_directories(mOutDir, ec);
	mRunLog.open(mOutDir + "/run.log", std::ios::trunc);

	mActive = true;
	Log("script loaded: " + path + " (" + std::to_string(mCommands.size()) + " commands)");
	return true;
}

void TestDriver::Log(const std::string& msg) {
	if (mRunLog.is_open()) {
		mRunLog << "[f" << mFrame << "] " << msg << "\n";
		mRunLog.flush();
	}
	LOG_INFO("AutoTest") << msg;   // Release 编译期裁掉，run.log 才是权威记录
}

void TestDriver::Fail(const std::string& reason) {
	const std::string op = (mIndex < mCommands.size())
		? mCommands[mIndex].value("op", "?") : "?";
	Log("FAIL at cmd#" + std::to_string(mIndex) + " (" + op + "): " + reason);
	LOG_ERROR("AutoTest") << "FAIL at cmd#" << mIndex << " (" << op << "): " << reason;
	mExitCode = 1;
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Finish() {
	Log("script finished OK");
	mActive = false;
	GameAPP::GetInstance().SetRunning(false);
}

void TestDriver::Update() {
	if (!mActive) return;
	++mFrame;
	mBreakFrame = false;
	int guard = 0;
	while (mActive && !mBreakFrame && mIndex < mCommands.size()) {
		if (!ExecuteCurrent()) break;          // 等待中，下帧重试
		Log("done cmd#" + std::to_string(mIndex) + " (" +
			mCommands[mIndex].value("op", "?") + ")");
		++mIndex;
		mWaitAccum = 0.0f;
		mFramesLeft = -1;
		mTimeoutAccum = 0.0f;
		if (++guard > 64) break;               // 单帧推进上限，防脚本自旋
	}
	if (mActive && mIndex >= mCommands.size()) Finish();
}

bool TestDriver::ExecuteCurrent() {
	const auto& cmd = mCommands[mIndex];
	const std::string op = cmd.value("op", "");

	// 等待型命令的超时看门狗（墙钟语义，不受 timescale 影响）
	mTimeoutAccum += DeltaTime::GetUnscaledDeltaTime();
	const float timeout = cmd.value("timeout", 15.0f);
	if (mTimeoutAccum > timeout) {
		Fail("timeout (" + std::to_string(timeout) + "s)");
		return false;
	}

	if (op == "wait_seconds") {
		mWaitAccum += DeltaTime::GetDeltaTime();
		return mWaitAccum >= cmd.value("value", 0.0f);
	}
	if (op == "wait_frames") {
		if (mFramesLeft < 0) mFramesLeft = cmd.value("value", 0);
		return --mFramesLeft < 0;
	}
	if (op == "goto_level") {
		if (!cmd.contains("level")) { Fail("goto_level 缺 level 字段"); return false; }
		auto& sm = SceneManager::GetInstance();
		sm.SetGlobalData("EnterLevel", std::to_string(cmd["level"].get<int>()));
		if (!sm.SwitchTo("GameScene")) { Fail("SwitchTo(GameScene) 失败"); return false; }
		return true;
	}
	if (op == "set_timescale") {
		DeltaTime::SetTimeScale(cmd.value("value", 1.0f));
		return true;
	}
	if (op == "quit") {
		Log("done cmd#" + std::to_string(mIndex) + " (quit)");
		Finish();
		return false;   // Finish 已停机，不再推进
	}

	Fail("未知命令 op=\"" + op + "\"");
	return false;
}
