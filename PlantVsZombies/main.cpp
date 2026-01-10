#define SDL_MAIN_HANDLED
#include "./CrashHandler.h"
#include "./GameRandom.h"
#include "./GameAPP.h"
#include <iostream>

int main(int argc, char* argv[])
{
    CrashHandler::Initialize();
    GameRandom::RandomizeSeed();

    int result = GameAPP::GetInstance().Run();

    CrashHandler::Cleanup();
        
    return result;
}