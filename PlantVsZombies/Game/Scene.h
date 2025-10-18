#pragma once
#ifndef _SCENE_H
#define _SCENE_H

#include "../UI/UIManager.h"
#include <SDL2/SDL.h>
#include <string>
#include <vector>
#include <algorithm>
#include <memory>

// ͼƬ��Ϣ�ṹ
struct TextureInfo {
    SDL_Texture* texture;
    int posX;
    int posY;
    float scaleX;       // X�����ű�����Ĭ��Ϊ1.0��
    float scaleY;       // Y�����ű�����Ĭ��Ϊ1.0��
    bool visible;       // �Ƿ�ɼ�
    int drawOrder;      // ����˳����ֵС���Ȼ��ƣ�
    std::string name;   // ��������

    TextureInfo(SDL_Texture* tex = nullptr, int x = 0, int y = 0, const std::string& n = "")
        : texture(tex), posX(x), posY(y), scaleX(1.0f), scaleY(1.0f), visible(true), drawOrder(0), name(n) {
    }
};

class Scene 
{
public:
    std::string name;
    virtual ~Scene() = default;

    virtual void OnEnter()  // ���볡��ʱ����
    {

    }          
    virtual void OnExit()    // �˳�����ʱ����  
    {
        uiManager_.ClearAll();
    }          
    virtual void Update() 
    {

    }
    virtual void Draw(SDL_Renderer* renderer)
    {
		DrawAllTextures(renderer);
    }

    virtual void HandleEvent(SDL_Event& event, InputHandler& input)
    {
        uiManager_.ProcessMouseEvent(&event, &input);
    }

protected:
    UIManager uiManager_;

    // ������������б�
    void AddTexture(const std::string& textureName, int posX, int posY, float scaleX = 1.0f, float scaleY = 1.0f, int drawOrder = 0);

    // �ӻ����б��Ƴ�����
    void RemoveTexture(const std::string& textureName);

    // ��������λ��
    void SetTexturePosition(const std::string& textureName, int posX, int posY);

    // ������������
    void SetTextureScale(const std::string& textureName, float scaleX, float scaleY);
    
    // ������������X
    void SetTextureScaleX(const std::string& textureName, float scaleX);

    // ������������Y
    void SetTextureScaleY(const std::string& textureName, float scaleY);

    // ��������ɼ���
    void SetTextureVisible(const std::string& textureName, bool visible);

    // �����������˳��
    void SetTextureDrawOrder(const std::string& textureName, int drawOrder);

    // �����������
    void ClearAllTextures();

    // ������������
    void DrawAllTextures(SDL_Renderer* renderer);

    // ��ȡ������Ϣ�����ڵ��Ի����⴦��
    TextureInfo* GetTextureInfo(const std::string& textureName);

private:
    std::vector<TextureInfo> mTextures;
};

#endif