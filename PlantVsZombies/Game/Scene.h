#pragma once
#ifndef _SCENE_H
#define _SCENE_H

#include <SDL2/SDL.h>
#include <string>

class Scene {
public:
    std::string name;
    virtual ~Scene() = default;

    virtual void OnEnter() {}           // 进入场景时调用
    virtual void OnExit() {}            // 退出场景时调用  
    virtual void Update() = 0;
    virtual void Draw(SDL_Renderer* renderer) = 0;
    virtual void HandleEvent(SDL_Event& event) = 0;

};

#endif