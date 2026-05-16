#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include "GameMonitor.h"
#include <SDL2/SDL.h>
#include <string>
#include <iostream>

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
            std::cout << "Debug模式已启用" << std::endl;
        }
    }

    int result = GameAPP::GetInstance().Run();

    CrashHandler::Cleanup();

    return result;
}
