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
#if defined(_WIN32)
	system("chcp 65001");   // 设控制台 UTF-8 代码页：仅 Windows 需要
	system("cls");
#endif
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
			LOG_WARN("Main") << "Debug模式已启用. 可能导致游戏不稳定等问题!";
		}
		else if (arg == "-NoInstance" || arg == "-noinstance")
		{
			GameAPP::mDisableInstancePath = true;
			LOG_WARN("Main") << "GPU Instance Path 已禁用 (A/B baseline). 可能会消除部分兼容性问题.";
		}
		else if (arg == "-Develop" || arg == "-develop")
		{
			GameAPP::mDevelopMode = true;
			LOG_WARN("Main") << "开发者模式已启用 (-develop). 你可以在游戏中按RSHIFT打开操作见面.";
		}
		else if (arg == "-Profile" || arg == "-profile")
		{
			g_ProfileEnabled = true;
			LOG_WARN("Main") << "性能分析输出已启用 (-profile). 可能导致游戏不稳定等问题!";
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