#include "Component.h"
#include "GameObject.h"

std::shared_ptr<GameObject> Component::GetGameObject() const {
    return gameObjectWeak.lock();
}

void Component::SetGameObject(std::shared_ptr<GameObject> obj) {
    gameObjectWeak = obj;
}