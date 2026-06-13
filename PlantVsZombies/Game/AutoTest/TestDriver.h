#pragma once
#ifndef _TESTDRIVER_H
#define _TESTDRIVER_H
#include <string>
#include <vector>
#include <fstream>
#include <nlohmann/json.hpp>

// -AutoTest 脚本自动驾驶：解析 JSON 命令队列，挂在主循环每帧推进。
// 设计文档：docs/superpowers/specs/2026-06-12-autotest-suite-design.md
class TestDriver {
public:
	static TestDriver& GetInstance();

	// 解析脚本并创建输出目录 ./autotest/out/<脚本名>/。失败返回 false（main 直接退出）。
	bool LoadScript(const std::string& path);

	bool IsActive() const { return mActive; }
	int  ExitCode() const { return mExitCode; }

	// 每帧调用（GameAPP::Run 中 sceneManager.Update() 之后）。未激活时立即返回。
	void Update();

	const std::string& OutDir() const { return mOutDir; }

private:
	TestDriver() = default;

	// 执行当前命令。返回 true = 已完成可推进下一条；false = 等待中（下帧重试）。
	bool ExecuteCurrent();

	void Fail(const std::string& reason);   // 记日志、退出码=1、结束游戏循环
	void Finish();                          // 全部命令跑完，正常收尾
	void Log(const std::string& msg);       // 写 run.log（带帧号）并 flush

	bool mActive = false;
	int  mExitCode = 0;
	size_t mIndex = 0;
	std::vector<nlohmann::json> mCommands;
	std::string mOutDir;
	std::ofstream mRunLog;
	unsigned long long mFrame = 0;

	// 等待型命令的逐命令状态（推进到下一条时清零）
	float mWaitAccum = 0.0f;     // wait_seconds 已累计（缩放后游戏时间）
	int   mFramesLeft = -1;      // wait_frames 剩余（-1 = 未初始化）
	float mTimeoutAccum = 0.0f;  // 当前命令已耗时（未缩放，墙钟语义），超 timeout 判失败
	bool  mBreakFrame = false;   // screenshot 等需要"本帧到此为止"的命令置位
	int   mInputPhase = -1;      // click/key(press) 跨帧状态机阶段（-1 = 未初始化）
};

#endif
