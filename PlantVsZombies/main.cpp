#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include <iostream>

int main(int argc, char** argv)
{
    system("chcp 65001");
    system("cls");
    CrashHandler::Initialize();
    GameRandom::RandomizeSeed();

    // 检查命令行参数是否包含 -Debug 后缀
    for (int i = 1; i < argc; ++i)
    {
        if (std::string(argv[i]) == "-Debug")
        {
            GameAPP::mDebugMode = true;
            GameAPP::mShowColliders = true;
            std::cout << "Debug模式已启用" << std::endl;
            break;
        }
    }

    int result = GameAPP::GetInstance().Run();

    CrashHandler::Cleanup();
        
    return result;
}