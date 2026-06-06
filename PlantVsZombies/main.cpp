#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include "GameMonitor.h"
#include "Logger.h"
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
	}

	int result = GameAPP::GetInstance().Run();

	CrashHandler::Cleanup();

	return result;
}