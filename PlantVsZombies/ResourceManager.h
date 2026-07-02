#pragma once
#ifndef _RESOURCEMANAGER_H
#define _RESOURCEMANAGER_H

#include "./Reanimation/Reanimation.h"
#include "./Reanimation/AnimationTypes.h"
#include "ResourcesXMLConfigReader.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_mixer.h>
#include <cstdint>
#include <string>
#include <unordered_map>
#include <memory>
#include <iostream>
#include <list>

namespace pvz { class VulkanTexturePool; struct VulkanTexture; }

// 纹理信息：id = bindless slot 索引；vkTex 为拥有的底层 VulkanTexture（Unload 时回收槽位 + 销毁 image）
struct Texture {
	uint32_t id = 0;           // Phase 3b: bindlessIndex（4096 槽位之一）
	int width = 0;
	int height = 0;
	// Phase 3b: 拥有的 VulkanTexture* —— 用于 Unload 时回收 bindless 槽位 + 销毁 image
	pvz::VulkanTexture* vkTex = nullptr;

	// —— 图集映射 ——
	// 若 atlasPage != nullptr，说明本纹理已被打进 atlasPage 指向的图集页，
	// 绘制时应改用 atlasPage->id，并把 [0,1] UV 重映射到 [aU0,aU1]x[aV0,aV1]。
	const Texture* atlasPage = nullptr;
	float aU0 = 0.0f, aV0 = 0.0f, aU1 = 1.0f, aV1 = 1.0f;
};

class ResourceManager {
private:
	// OpenGL 纹理缓存
	std::unordered_map<std::string, Texture> mTextures;

	// 动画缓存
	std::unordered_map<std::string, std::shared_ptr<Reanimation>> mReanimations;

	// reanim 纹理图集页（用 list 保证元素地址稳定，Texture::atlasPage 会指向其中元素）
	std::list<Texture> mAtlasPages;

	// 字体缓存（按字体名 -> 大小 -> TTF_Font*）
	std::unordered_map<std::string, std::unordered_map<int, TTF_Font*>> fonts;

	// 音效缓存
	std::unordered_map<std::string, Mix_Chunk*> sounds;

	// 音乐缓存
	std::unordered_map<std::string, Mix_Music*> music;

	// 配置读取器
	ResourcesXMLConfigReader configReader;

	static ResourceManager* instance;
	pvz::VulkanTexturePool* mTexturePool = nullptr;
	ResourceManager() {}

	// 加载分割贴图
	bool LoadTiledTextureGL(const TiledImageInfo& info, const std::string& prefix);

	// 把已解码的 ABGR8888 surface 上传 Vulkan 并插入 mTextures（接管 converted 所有权）。
	// 仅主线程调用；上传失败仍按原语义插入空 Texture 并返回其指针。
	const Texture* UploadDecodedTexture(SDL_Surface* converted, const std::string& key, const std::string& filepath);

public:
	static ResourceManager& GetInstance();
	static void ReleaseInstance();

	bool Initialize(const std::string& configPath = "./resources/resources.xml");

	// Phase 3b：由 GameApp 在初始化期注入。LoadTexture / LoadTiledTextureGL 会通过它上传像素并获取 bindless 槽位。
	void SetVulkanTexturePool(pvz::VulkanTexturePool* pool) { mTexturePool = pool; }

	// 加载纹理，返回纹理信息指针，失败返回 nullptr
	const Texture* LoadTexture(const std::string& filepath, const std::string& key = "");
	// 获取已加载的纹理，不存在返回 nullptr。
	// warnOnMiss=false 供"存在性探测"调用方使用（如 Reanimation 加载时先查后载），
	// 这类 miss 是预期的正常路径，由调用方自行处理，不应记 WARN。
	const Texture* GetTexture(const std::string& key, bool warnOnMiss = true) const;
	// 卸载单个纹理
	void UnloadTexture(const std::string& key);
	// 检查纹理是否存在
	bool HasTexture(const std::string& key) const;

	// 批量加载（使用配置信息）
	bool LoadAllGameImages();
	bool LoadAllParticleTextures();
	bool LoadAllFonts();
	bool LoadAllSounds();
	bool LoadAllMusic();
	bool LoadAllReanimations();
	// 把所有 reanim 引用到的部件纹理打进图集页，消除批渲染时的纹理单元抖动。
	// 必须在 GL 上下文就绪、且 LoadAllReanimations 之后调用。
	void BuildReanimAtlases();
	/// @brief 加载 ./resources/image/reanim/ 目录下的所有 JPG/PNG 图片，使用文件名（不含扩展名）作为键名
	/// @param directory 要扫描的目录，默认为 "./resources/image/reanim/"
	/// @return 是否全部加载成功
	bool LoadAllImagesFromPath(const std::string& directory = "./resources/image/reanim/");

	// ---------- 动画管理 ----------
	std::shared_ptr<Reanimation> LoadReanimation(const std::string& key, const std::string& path);
	std::shared_ptr<Reanimation> GetReanimation(const std::string& key);
	void UnloadReanimation(const std::string& key);
	bool HasReanimation(const std::string& key) const;
	std::string AnimationTypeToString(AnimationType type);

	// ---------- 字体管理 ----------
	bool LoadFont(const std::string& path, const std::string& key = "");
	TTF_Font* GetFont(const std::string& key, int size);
	void UnloadFont(const std::string& key);
	void UnloadFontSize(const std::string& key, int size);
	void CleanupUnusedFontSizes();
	bool HasFont(const std::string& key) const;

	// ---------- 音效管理 ----------
	Mix_Chunk* LoadSound(const std::string& path, const std::string& key = "");
	Mix_Chunk* GetSound(const std::string& key);
	void UnloadSound(const std::string& key);
	bool HasSound(const std::string& key) const;

	// ---------- 音乐管理 ----------
	Mix_Music* LoadMusic(const std::string& path, const std::string& key = "");
	Mix_Music* GetMusic(const std::string& key);
	void UnloadMusic(const std::string& key);
	bool HasMusic(const std::string& key) const;

	// ---------- 辅助函数 ----------
	std::string GenerateStandardKey(const std::string& path, const std::string& prefix);
	std::vector<int> GetLoadedFontSizes(const std::string& key) const;
	int GetLoadedFontCount() const;

	// ---------- 获取配置信息（供外部使用）----------
	const std::vector<TiledImageInfo>& GetGameImageInfos() const { return configReader.GetGameImageInfos(); }
	const std::vector<TiledImageInfo>& GetParticleTextureInfos() const { return configReader.GetParticleTextureInfos(); }
	const std::vector<std::string>& GetSoundPaths() const { return configReader.GetSoundPaths(); }
	const std::vector<std::string>& GetMusicPaths() const { return configReader.GetMusicPaths(); }
	const std::unordered_map<std::string, std::string>& GetAnimationPaths() const { return configReader.GetReanimationPaths(); }

	// 清理所有资源
	void UnloadAll();

	ResourceManager(const ResourceManager&) = delete;
	ResourceManager& operator=(const ResourceManager&) = delete;
};

#endif