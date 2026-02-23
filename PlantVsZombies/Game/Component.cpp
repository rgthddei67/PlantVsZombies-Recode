#include "Component.h"
#include "GameObject.h"

std::shared_ptr<GameObject> Component::GetGameObject() const {
    return mGameObjectWeak.lock();
}

void Component::SetGameObject(std::shared_ptr<GameObject> obj) {
    mGameObjectWeak = obj;
}