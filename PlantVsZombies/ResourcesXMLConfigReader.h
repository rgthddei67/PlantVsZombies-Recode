#pragma once
#ifndef _RESOURCESXMLCONFIG_H
#define _RESOURCESXMLCONFIG_H

#include "pugixml.hpp"
#include <vector>
#include <string>
#include <unordered_map>
#include <iostream>

// 分份贴图信息
struct TiledImageInfo {
    std::string path;
    int columns = 1;  // 水平分割数，默认1
    int rows = 1;     // 垂直分割数，默认1
};

class ResourcesXMLConfigReader {
private:
    std::vector<TiledImageInfo> gameImageInfos;
    std::vector<TiledImageInfo> particleTextureInfos;
    std::vector<std::string> fontPaths;
    std::vector<std::string> soundPaths;
    std::vector<std::string> musicPaths;
    std::unordered_map<std::string, std::string> reanimationPaths;

    bool isLoaded;

public:
    ResourcesXMLConfigReader() : isLoaded(false) {}

    bool LoadConfig(const std::string& xmlPath) {
        pugi::xml_document doc;
        pugi::xml_parse_result result = doc.load_file(xmlPath.c_str());

        if (!result) {
            std::cerr << "加载XML配置文件失败: " << xmlPath
                << "，错误: " << result.description() << std::endl;
            return false;
        }

        pugi::xml_node root = doc.child("Resources");
        if (!root) {
            std::cerr << "无效的XML配置文件结构" << std::endl;
            return false;
        }

        // 清空旧数据
        gameImageInfos.clear();
        particleTextureInfos.clear();
        fontPaths.clear();
        soundPaths.clear();
        musicPaths.clear();
        reanimationPaths.clear();

        // 解析 GameImages
        pugi::xml_node gameImagesNode = root.child("GameImages");
        if (gameImagesNode) {
            for (pugi::xml_node imageNode : gameImagesNode.children("Image")) {
                TiledImageInfo info;
                info.path = imageNode.text().get();
                info.columns = imageNode.attribute("Column").as_int(1);  // 默认1
                info.rows = imageNode.attribute("Row").as_int(1);        // 默认1
                gameImageInfos.push_back(info);
            }
        }

        // 解析 ParticleTextures
        pugi::xml_node particlesNode = root.child("ParticleTextures");
        if (particlesNode) {
            for (pugi::xml_node textureNode : particlesNode.children("Texture")) {
                TiledImageInfo info;
                info.path = textureNode.text().get();
                info.columns = textureNode.attribute("Column").as_int(1);
                info.rows = textureNode.attribute("Row").as_int(1);
                particleTextureInfos.push_back(info);
            }
        }

        // 读取字体
        pugi::xml_node fontsNode = root.child("Fonts");
        if (fontsNode) {
            for (pugi::xml_node fontNode : fontsNode.children("Font")) {
                fontPaths.push_back(fontNode.text().get());
            }
        }

        // 读取音效
        pugi::xml_node soundsNode = root.child("Sounds");
        if (soundsNode) {
            for (pugi::xml_node soundNode : soundsNode.children("Sound")) {
                soundPaths.push_back(soundNode.text().get());
            }
        }

        // 读取音乐
        pugi::xml_node musicNode = root.child("Music");
        if (musicNode) {
            for (pugi::xml_node trackNode : musicNode.children("Track")) {
                musicPaths.push_back(trackNode.text().get());
            }
        }

        // 读取动画资源
        pugi::xml_node reanimationsNode = root.child("Reanimations");
        if (reanimationsNode) {
            for (pugi::xml_node reanimNode : reanimationsNode.children("Reanimation")) {
                std::string name = reanimNode.attribute("name").value();
                std::string path = reanimNode.text().get();
                if (!name.empty() && !path.empty()) {
                    reanimationPaths[name] = path;
                }
            }
        }

        isLoaded = true;

#ifdef _DEBUG
        std::cout << "XML配置加载成功:" << std::endl;
        std::cout << "  游戏图片: " << gameImageInfos.size() << " 个" << std::endl;
        std::cout << "  粒子纹理: " << particleTextureInfos.size() << " 个" << std::endl;
        std::cout << "  字体: " << fontPaths.size() << " 个" << std::endl;
        std::cout << "  音效: " << soundPaths.size() << " 个" << std::endl;
        std::cout << "  音乐: " << musicPaths.size() << " 个" << std::endl;
        std::cout << "  动画: " << reanimationPaths.size() << " 个" << std::endl;
#endif

        return true;
    }

    // 获取方法
    const std::vector<TiledImageInfo>& GetGameImageInfos() const { return gameImageInfos; }
    const std::vector<TiledImageInfo>& GetParticleTextureInfos() const { return particleTextureInfos; }
    const std::vector<std::string>& GetFontPaths() const { return fontPaths; }
    const std::vector<std::string>& GetSoundPaths() const { return soundPaths; }
    const std::vector<std::string>& GetMusicPaths() const { return musicPaths; }
    const std::unordered_map<std::string, std::string>& GetReanimationPaths() const { return reanimationPaths; }

    bool IsLoaded() const { return isLoaded; }
};
#endif