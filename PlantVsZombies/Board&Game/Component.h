#pragma once
#ifndef _COMPONENT_H
#define _COMPONENT_H

#include "Definit.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <typeindex>
#include <typeinfo>
#include <functional>
#include <vector>

class GameObject;

class Component : public std::enable_shared_from_this<Component> {
public:
	std::weak_ptr<GameObject> gameObjectWeak;   // ����shared_ptr ��ֹѭ������(GameObject)
    bool enabled = true;

    virtual ~Component() = default;

    // �������ڷ���
    virtual void Start() {}           // �����ʼʱ����
    virtual void Update() {}          // ÿ֡����
    virtual void OnDestroy() {}       // �������ʱ����

    // ��ȡ GameObject����������ڣ�
    std::shared_ptr<GameObject> GetGameObject() const;

    // ����������Ϸ����
    void SetGameObject(std::shared_ptr<GameObject> obj);

    // ���Ʒ�������ѡ��
    virtual void Draw(SDL_Renderer* renderer) {}
};

#endif