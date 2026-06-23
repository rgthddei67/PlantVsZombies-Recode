#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameApp.h"
#include "GameMonitor.h"
#include "Logger.h"
#include "./Profiler.h"
#include "./Game/AutoTest/TestDriver.h"
#include <SDL2/SDL.h>
#include <string>

int main(int argc, char** argv)
{
	system("chcp 65001");
	system("cls");
	CrashHandler::Initialize();
	GameRandom::RandomizeSeed();

	GameMonitor::Init();

	// 检查命令行参数
	std::string autoTestScript;
	for (int i = 1; i < argc; ++i)
	{
		const std::string arg = argv[i];
		if (arg == "-Debug" || arg == "-debug")
		{
			GameAPP::mDebugMode = true;
			GameAPP::mShowColliders = true;
			LOG_DEBUG("Main") << "Debug模式已启用";
		}
		else if (arg == "-NoInstance" || arg == "-noinstance")
		{
			GameAPP::mDisableInstancePath = true;
			LOG_DEBUG("Main") << "GPU instance path 已禁用 (A/B baseline)";
		}
		else if (arg == "-Profile" || arg == "-profile")
		{
			g_ProfileEnabled = true;
			LOG_DEBUG("Main") << "性能分析输出已启用 (-Profile)";
		}
		else if ((arg == "-AutoTest" || arg == "-autotest") && i + 1 < argc)
		{
			autoTestScript = argv[++i];
			GameAPP::mAutoTestMode = true;
		}
		else if ((arg == "-Seed" || arg == "-seed") && i + 1 < argc)
		{
			try {
				GameRandom::SetSeed(std::stoull(argv[++i]));
			}
			catch (const std::exception& e) {
				LOG_WARN("Main") << "-Seed 参数无效，已忽略: " << e.what();
			}
		}
	}

	if (GameAPP::mAutoTestMode) {
		if (!TestDriver::GetInstance().LoadScript(autoTestScript)) {
			CrashHandler::Cleanup();
			return 100;   // 脚本解析失败
		}
	}

	int result = GameAPP::GetInstance().Run();

	if (GameAPP::mAutoTestMode && result == 0) {
		result = TestDriver::GetInstance().ExitCode();
	}

	CrashHandler::Cleanup();

	return result;
}