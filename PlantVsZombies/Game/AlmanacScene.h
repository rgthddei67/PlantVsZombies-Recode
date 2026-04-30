#pragma once
#ifndef _ALMANAC_SCENE_H
#define _ALMANAC_SCENE_H

#include "Scene.h"

class Plant;
class Zombie;

class AlmanacScene : public Scene {
private:
	std::shared_ptr<Button> mPlantButton;
    std::shared_ptr<Button> mZombieButton;
    std::shared_ptr<Button> mBackMenuButton;
    
    std::weak_ptr<Plant> mPlant;
    std::weak_ptr<Zombie> mZombie;

public:
    void OnEnter() override;
    void OnExit() override;
    void Update() override;

    bool mReadyToSwitchMainMenu = false;
    bool mReadyToSwitchPlantScene = false;
    bool mReadyToSwitchZombieScene = false;

protected:
    void BuildDrawCommands() override;
};

#endif