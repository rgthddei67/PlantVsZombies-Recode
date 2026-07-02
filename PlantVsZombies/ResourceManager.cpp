#include "ResourceManager.h"
#include "./Game/Plant/GameDataManager.h"
#include "./Renderer/VulkanTexturePool.h"
#include "Logger.h"
#include "FileManager.h"
#include <algorithm>
#include <cctype>
#include <unordered_set>
#include <vector>

ResourceManager* ResourceManager::instance = nullptr;

namespace {
	// worker 线程可安全调用：只做 打开→解码→转 ABGR8888，不碰共享状态、不打日志。
	// 失败时 surface==nullptr，error 携带与旧 LoadTexture 逐字一致的日志文案，由主线程统一输出。
	struct DecodedImage {
		SDL_Surface* surface = nullptr;
		std::string error;
	};

	DecodedImage DecodeImageFile(const std::string& filepath) {
		DecodedImage out;
		SDL_RWops* rw = SDL_RWFromFile(filepath.c_str(), "rb");
		if (!rw) {
			out.error = "LoadTexture 无法打开图片: " + filepath;
			return out;
		}
		SDL_Surface* surface = IMG_Load_RW(rw, 1);   // freesrc=1
		if (!surface) {
			out.error = "LoadTexture 无法加载图片: " + filepath + " - " + IMG_GetError();
			return out;
		}
		// 统一转换为 RGBA，解决对齐与源格式不确定问题
		SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
		SDL_FreeSurface(surface);
		if (!converted) {
			out.error = "LoadTexture 无法转换图片格式: " + filepath;
			return out;
		}
		out.surface = converted;
		return out;
	}
}

ResourceManager& ResourceManager::GetInstance() {
	if (!instance) {
		instance = new ResourceManager();
	}
	return *instance;
}

void ResourceManager::ReleaseInstance() {
	if (instance) {
		instance->UnloadAll();
		delete instance;
		instance = nullptr;
	}
}

bool ResourceManager::Initialize(const std::string& configPath) {
	return configReader.LoadConfig(configPath);
}

const Texture* ResourceManager::LoadTexture(const std::string& filepath, const std::string& key) {
	std::string actualKey = key.empty() ? filepath : key;
	auto it = mTextures.find(actualKey);
	if (it != mTextures.end()) {
		return &it->second;
	}

	DecodedImage decoded = DecodeImageFile(filepath);
	if (!decoded.surface) {
		LOG_ERROR("ResourceManager") << decoded.error;
		return nullptr;
	}
	return UploadDecodedTexture(decoded.surface, actualKey, filepath);
}

const Texture* ResourceManager::UploadDecodedTexture(SDL_Surface* converted, const std::string& key, const std::string& filepath) {
	Texture tex;
	tex.width = converted->w;
	tex.height = converted->h;

	if (mTexturePool) {
		pvz::VulkanTexture* vkt = mTexturePool->CreateTextureRGBA8(converted->w, converted->h, converted->pixels);
		if (vkt) {
			tex.vkTex = vkt;
			tex.id = vkt->bindlessIndex;
		}
		else {
			LOG_ERROR("ResourceManager") << "LoadTexture VulkanTexturePool 上传失败: " << filepath;
		}
	}
	SDL_FreeSurface(converted);

	mTextures[key] = tex;
	return &mTextures[key];
}

const Texture* ResourceManager::GetTexture(const std::string& key, bool warnOnMiss) const {
	auto it = mTextures.find(key);
	if (it != mTextures.end()) {
		return &it->second;
	}
	// 与 GetSound/GetMusic 一致：miss 默认记 WARN，否则裸键名拼错时会变成看不见的 bug。
	// 但"存在性探测"调用方（warnOnMiss=false）的 miss 是预期路径，由其自身处理，不在此告警。
	if (warnOnMiss) {
		LOG_WARN("ResourceManager") << "纹理未找到: " << key;
	}
	return nullptr;
}

void ResourceManager::UnloadTexture(const std::string& key) {
	auto it = mTextures.find(key);
	if (it != mTextures.end()) {
		if (mTexturePool && it->second.vkTex) {
			mTexturePool->DestroyTexture(it->second.vkTex);
		}
		mTextures.erase(it);
		LOG_DEBUG("ResourceManager") << "卸载纹理: " << key;
	}
}

bool ResourceManager::HasTexture(const std::string& key) const {
	return mTextures.find(key) != mTextures.end();
}

bool ResourceManager::LoadTiledTextureGL(const TiledImageInfo& info, const std::string& prefix) {
	// 加载原图到表面（用于分割）
	SDL_RWops* rw = SDL_RWFromFile(info.path.c_str(), "rb");
	if (!rw) {
		LOG_ERROR("ResourceManager") << "LoadTiledTexture 无法打开图片: " << info.path;
		return false;
	}
	SDL_Surface* loadedSurface = IMG_Load_RW(rw, 1);   // freesrc=1
	if (!loadedSurface) {
		LOG_ERROR("ResourceManager") << "LoadTiledTexture 无法加载图片: " << info.path << " - " << IMG_GetError();
		return false;
	}

	// 切片前统一转为 ABGR8888，统一处理索引色/灰度/RGB 等源格式，
	// 否则 SDL_CreateRGBSurface 用源 BPP/Rmask 复制时不会带调色板，
	// 导致索引色 PNG 切出来的子图全黑/全透明
	SDL_Surface* originalSurface = SDL_ConvertSurfaceFormat(loadedSurface, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(loadedSurface);
	if (!originalSurface) {
		LOG_ERROR("ResourceManager") << "LoadTiledTexture 无法转换源图片格式: " << info.path << " - " << SDL_GetError();
		return false;
	}
	// 关闭 src 的 alpha-blend，blit 走纯拷贝，避免与 dst 全 0 alpha 混合丢失
	SDL_SetSurfaceBlendMode(originalSurface, SDL_BLENDMODE_NONE);

	int imgW = originalSurface->w;
	int imgH = originalSurface->h;
	int tileW = imgW / info.columns;
	int tileH = imgH / info.rows;

	if (imgW % info.columns != 0 || imgH % info.rows != 0) {
		LOG_WARN("ResourceManager") << "图片尺寸 " << imgW << "x" << imgH
			<< " 不能被 " << info.columns << "x" << info.rows
			<< " 整除，可能产生边缘裁剪";
	}

	std::string baseKey = GenerateStandardKey(info.path, prefix);
	bool success = true;

	for (int row = 0; row < info.rows; ++row) {
		for (int col = 0; col < info.columns; ++col) {
			// 直接创建 ABGR8888 子表面（源已统一为该格式）
			SDL_Surface* tileSurface = SDL_CreateRGBSurfaceWithFormat(
				0, tileW, tileH, 32, SDL_PIXELFORMAT_ABGR8888);
			if (!tileSurface) {
				LOG_ERROR("ResourceManager") << "LoadTiledTexture 无法创建子表面: " << SDL_GetError();
				success = false;
				continue;
			}

			SDL_Rect srcRect = { col * tileW, row * tileH, tileW, tileH };
			if (SDL_BlitSurface(originalSurface, &srcRect, tileSurface, nullptr) != 0) {
				LOG_ERROR("ResourceManager") << "LoadTiledTexture Blit失败: " << SDL_GetError();
				SDL_FreeSurface(tileSurface);
				success = false;
				continue;
			}

			Texture tex;
			tex.width = tileW;
			tex.height = tileH;
			if (mTexturePool) {
				pvz::VulkanTexture* vkt = mTexturePool->CreateTextureRGBA8(tileW, tileH, tileSurface->pixels);
				if (vkt) {
					tex.vkTex = vkt;
					tex.id = vkt->bindlessIndex;
				}
				else {
					LOG_ERROR("ResourceManager") << "LoadTiledTexture 分割贴图上传失败: " << info.path << " tile " << row << "," << col;
					success = false;
				}
			}
			SDL_FreeSurface(tileSurface);

			int index = row * info.columns + col;
			std::string key = baseKey + "_PART_" + std::to_string(index);
			mTextures[key] = tex;

		}
	}

	SDL_FreeSurface(originalSurface);
	return success;
}

bool ResourceManager::LoadAllGameImages() {
	bool success = true;
	const auto& infos = GetGameImageInfos();
	for (const auto& info : infos) {
		if (info.columns <= 1 && info.rows <= 1) {
			// 普通图片
			std::string key = GenerateStandardKey(info.path, "IMAGE_");
			if (!LoadTexture(info.path, key)) {
				LOG_ERROR("ResourceManager") << "加载游戏图片失败: " << info.path;
				success = false;
			}
		}
		else {
			// 分割贴图
			if (!LoadTiledTextureGL(info, "IMAGE_")) {
				LOG_ERROR("ResourceManager") << "加载分割贴图失败: " << info.path;
				success = false;
			}
		}
	}
	return success;
}

bool ResourceManager::LoadAllParticleTextures() {
	bool success = true;
	const auto& infos = GetParticleTextureInfos();
	for (const auto& info : infos) {
		if (info.columns <= 1 && info.rows <= 1) {
			std::string key = GenerateStandardKey(info.path, "PARTICLE_");
			if (!LoadTexture(info.path, key)) {
				LOG_ERROR("ResourceManager") << "加载粒子纹理失败: " << info.path;
				success = false;
			}
		}
		else {
			if (!LoadTiledTextureGL(info, "PARTICLE_")) {
				LOG_ERROR("ResourceManager") << "加载粒子分割贴图失败: " << info.path;
				success = false;
			}
		}
	}
	return success;
}

bool ResourceManager::LoadAllFonts()
{
	bool success = true;
	const auto& paths = configReader.GetFontPaths();
	for (const auto& path : paths)
	{
		if (!LoadFont(path))
		{
			LOG_ERROR("ResourceManager") << "注册字体失败: " << path;
			success = false;
		}
	}
	return success;
}

bool ResourceManager::LoadAllSounds()
{
	bool success = true;
	const auto& paths = configReader.GetSoundPaths();
	for (const auto& path : paths)
	{
		std::string key = GenerateStandardKey(path, "SOUND_");
		if (!LoadSound(path, key))
		{
			LOG_ERROR("ResourceManager") << "加载音效失败: " << path;
			success = false;
		}
	}
	return success;
}

bool ResourceManager::LoadAllMusic()
{
	bool success = true;
	const auto& paths = configReader.GetMusicPaths();
	for (const auto& path : paths)
	{
		std::string key = GenerateStandardKey(path, "MUSIC_");
		if (!LoadMusic(path, key))
		{
			LOG_ERROR("ResourceManager") << "加载音乐失败: " << path;
			success = false;
		}
	}
	return success;
}

bool ResourceManager::LoadAllReanimations()
{
	bool success = true;
	const auto& reanimPaths = configReader.GetReanimationPaths();
	for (const auto& reanimPair : reanimPaths) {
		const std::string& key = reanimPair.first;
		const std::string& path = reanimPair.second;

		if (LoadReanimation(key, path)) {
		}
		else {
			success = false;
			LOG_ERROR("ResourceManager") << "加载动画失败: " << key << " from " << path;
		}
	}
	return success;
}

void ResourceManager::BuildReanimAtlases() {
	// Phase 3a stub：图集化原本通过 GL FBO blit 把多张源纹理拼到一张图集页上，
	// 消除 batch 渲染时的 32 纹理单元抖动。Vulkan bindless 一开始就没有 32 单元
	// 限制，因此 3b+ 这个函数应该可以整体删除（计划文档 risk #2 也提到要换成
	// CPU-side compose into SDL_Surface 的简化做法，待 3b 评估）。
	// 现阶段保留空函数体，避免调用方崩溃。
}

bool ResourceManager::LoadAllImagesFromPath(const std::string& directory) {
	bool allSuccess = true;
	int loadedCount = 0;

	// 文件名来自构建期烘焙的清单（FileManager::ListResourceFiles 经 SDL_RWops 读，APK 可读），
	// 不再用 std::filesystem 枚举，使该路径在 Android 上也能列举 APK assets。
	// 返回的全路径与旧 directory_iterator 逐字一致，下游加载零差异。
	std::vector<std::string> entries = FileManager::ListResourceFiles(directory);

	for (const std::string& fullPath : entries) {
		std::string ext = FileManager::GetFileExtension(fullPath);
		// 转换为小写以统一判断
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

		if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
			// stem = 文件名去扩展名（ext 已小写但长度不变，故可直接按长度裁剪）
			std::string fileName = FileManager::GetFileName(fullPath);
			std::string key = fileName.substr(0, fileName.size() - ext.size());
			// 将 key 转换为大写
			std::transform(key.begin(), key.end(), key.begin(), ::toupper);
			// 添加前缀 "IMAGE_"
			key = "IMAGE_" + key;

			if (LoadTexture(fullPath, key)) {
				loadedCount++;
			}
			else {
				allSuccess = false;
			}
		}
	}

	if (loadedCount == 0) {
		LOG_ERROR("ResourceManager") << "在目录 " << directory << " 中没有找到任何 JPG/PNG 图片";
		return false;
	}

	return allSuccess;
}

std::shared_ptr<Reanimation> ResourceManager::LoadReanimation(const std::string& key, const std::string& path) {
	auto it = mReanimations.find(key);
	if (it != mReanimations.end()) {
		return it->second;
	}

	auto reanim = std::make_shared<Reanimation>();
	reanim->mResourceManager = this;
	if (!reanim->LoadFromFile(path)) {
		LOG_ERROR("ResourceManager") << "LoadReanimation 失败: " << path;
		return nullptr;
	}

	mReanimations[key] = reanim;
	return reanim;
}

std::shared_ptr<Reanimation> ResourceManager::GetReanimation(const std::string& key) {
	auto it = mReanimations.find(key);
	if (it != mReanimations.end()) {
		// 返回新的实例（深拷贝）
		auto cachedReanim = it->second;
		auto newReanim = std::make_shared<Reanimation>();
		newReanim->mFPS = cachedReanim->mFPS;
		newReanim->mIsLoaded = cachedReanim->mIsLoaded;
		newReanim->mResourceManager = cachedReanim->mResourceManager;
		newReanim->mTracks = cachedReanim->mTracks;
		return newReanim;
	}
	LOG_ERROR("ResourceManager") << "GetReanimation 未找到: " << key;
	return nullptr;
}

void ResourceManager::UnloadReanimation(const std::string& key) {
	mReanimations.erase(key);
}

bool ResourceManager::HasReanimation(const std::string& key) const {
	return mReanimations.find(key) != mReanimations.end();
}

std::string ResourceManager::AnimationTypeToString(AnimationType type) {
	return GameDataManager::GetInstance().GetAnimationName(type);
}

bool ResourceManager::LoadFont(const std::string& path, const std::string& key) {
	std::string actualKey = key.empty() ? path : key;
	if (fonts.find(actualKey) != fonts.end()) {
		return true; // 已注册
	}
	fonts[actualKey] = std::unordered_map<int, TTF_Font*>();
	return true;
}

TTF_Font* ResourceManager::GetFont(const std::string& key, int size) {
	if (key.empty() || size <= 0) {
		LOG_ERROR("ResourceManager") << "GetFont 无效的字体参数: key=" << key << ", size=" << size;
		return nullptr;
	}

	auto fontIt = fonts.find(key);
	if (fontIt == fonts.end()) {
		LOG_ERROR("ResourceManager") << "GetFont 字体未注册: '" << key << "'";
		return nullptr;
	}

	auto& sizeMap = fontIt->second;
	auto sizeIt = sizeMap.find(size);
	if (sizeIt != sizeMap.end()) {
		return sizeIt->second;
	}

	// 查找字体文件路径
	std::string fontPath;
	const auto& fontPaths = configReader.GetFontPaths();
	for (const auto& path : fontPaths) {
		std::string filename = path.substr(path.find_last_of("/\\") + 1);
		std::string fileKey = filename.substr(0, filename.find_last_of('.'));
		if (fileKey == key) {
			fontPath = path;
			break;
		}
	}
	if (fontPath.empty()) {
		// 尝试直接使用 key 作为路径
		fontPath = key;
	}

	SDL_RWops* fontRw = SDL_RWFromFile(fontPath.c_str(), "rb");
	if (!fontRw) {
		LOG_ERROR("ResourceManager") << "GetFont 无法打开字体: " << fontPath << " size: " << size;
		return nullptr;
	}
	TTF_Font* font = TTF_OpenFontRW(fontRw, 1, size);   // freesrc=1
	if (!font) {
		LOG_ERROR("ResourceManager") << "GetFont 加载字体失败: " << fontPath << " size: " << size << " - " << TTF_GetError();
		return nullptr;
	}

	sizeMap[size] = font;
	return font;
}

void ResourceManager::UnloadFont(const std::string& key) {
	auto it = fonts.find(key);
	if (it != fonts.end()) {
		for (auto& sizePair : it->second) {
			TTF_CloseFont(sizePair.second);
		}
		fonts.erase(it);
		LOG_DEBUG("ResourceManager") << "卸载字体: " << key;
	}
}

void ResourceManager::UnloadFontSize(const std::string& key, int size) {
	auto fontIt = fonts.find(key);
	if (fontIt != fonts.end()) {
		auto& sizeMap = fontIt->second;
		auto sizeIt = sizeMap.find(size);
		if (sizeIt != sizeMap.end()) {
			TTF_CloseFont(sizeIt->second);
			sizeMap.erase(sizeIt);
			LOG_DEBUG("ResourceManager") << "卸载字体: " << key << " size: " << size;
			if (sizeMap.empty()) {
				fonts.erase(fontIt);
			}
		}
	}
}

void ResourceManager::CleanupUnusedFontSizes() {
	for (auto it = fonts.begin(); it != fonts.end(); ) {
		if (it->second.empty()) {
			it = fonts.erase(it);
		}
		else {
			++it;
		}
	}
}

bool ResourceManager::HasFont(const std::string& key) const {
	return fonts.find(key) != fonts.end();
}

std::vector<int> ResourceManager::GetLoadedFontSizes(const std::string& key) const {
	std::vector<int> sizes;
	auto it = fonts.find(key);
	if (it != fonts.end()) {
		for (const auto& sizePair : it->second) {
			sizes.push_back(sizePair.first);
		}
	}
	return sizes;
}

int ResourceManager::GetLoadedFontCount() const {
	int total = 0;
	for (const auto& fontPair : fonts) {
		total += static_cast<int>(fontPair.second.size());
	}
	return total;
}

Mix_Chunk* ResourceManager::LoadSound(const std::string& path, const std::string& key) {
	std::string actualKey = key.empty() ? path : key;
	if (sounds.find(actualKey) != sounds.end()) {
		return sounds[actualKey];
	}

	SDL_RWops* sndRw = SDL_RWFromFile(path.c_str(), "rb");
	if (!sndRw) {
		LOG_ERROR("ResourceManager") << "LoadSound 无法打开音效: " << path;
		return nullptr;
	}
	Mix_Chunk* sound = Mix_LoadWAV_RW(sndRw, 1);   // freesrc=1
	if (!sound) {
		LOG_ERROR("ResourceManager") << "LoadSound 加载音效失败: " << path << " - " << Mix_GetError();
		return nullptr;
	}

	sounds[actualKey] = sound;
	return sound;
}

Mix_Chunk* ResourceManager::GetSound(const std::string& key) {
	auto it = sounds.find(key);
	if (it != sounds.end()) {
		return it->second;
	}
	LOG_WARN("ResourceManager") << "音效未找到: " << key;
	return nullptr;
}

void ResourceManager::UnloadSound(const std::string& key) {
	auto it = sounds.find(key);
	if (it != sounds.end()) {
		Mix_FreeChunk(it->second);
		sounds.erase(it);
		LOG_DEBUG("ResourceManager") << "卸载音效: " << key;
	}
}

bool ResourceManager::HasSound(const std::string& key) const {
	return sounds.find(key) != sounds.end();
}

Mix_Music* ResourceManager::LoadMusic(const std::string& path, const std::string& key) {
	std::string actualKey = key.empty() ? path : key;
	if (music.find(actualKey) != music.end()) {
		return music[actualKey];
	}

	SDL_RWops* musRw = SDL_RWFromFile(path.c_str(), "rb");
	if (!musRw) {
		LOG_ERROR("ResourceManager") << "LoadMusic 无法打开音乐: " << path;
		return nullptr;
	}
	Mix_Music* mus = Mix_LoadMUS_RW(musRw, 1);   // freesrc=1
	if (!mus) {
		LOG_ERROR("ResourceManager") << "LoadMusic 加载音乐失败: " << path << " - " << Mix_GetError();
		return nullptr;
	}

	music[actualKey] = mus;
	return mus;
}

Mix_Music* ResourceManager::GetMusic(const std::string& key) {
	auto it = music.find(key);
	if (it != music.end()) {
		return it->second;
	}
	LOG_WARN("ResourceManager") << "音乐未找到: " << key;
	return nullptr;
}

void ResourceManager::UnloadMusic(const std::string& key) {
	auto it = music.find(key);
	if (it != music.end()) {
		Mix_FreeMusic(it->second);
		music.erase(it);
		LOG_DEBUG("ResourceManager") << "卸载音乐: " << key;
	}
}

bool ResourceManager::HasMusic(const std::string& key) const {
	return music.find(key) != music.end();
}

std::string ResourceManager::GenerateStandardKey(const std::string& path, const std::string& prefix) {
	std::string filename = path.substr(path.find_last_of("/\\") + 1);
	std::string nameWithoutExt = filename.substr(0, filename.find_last_of('.'));
	std::transform(nameWithoutExt.begin(), nameWithoutExt.end(), nameWithoutExt.begin(), ::toupper);
	for (char& c : nameWithoutExt) {
		if (!std::isalnum(c)) {
			c = '_';
		}
	}
	return prefix + nameWithoutExt;
}

void ResourceManager::UnloadAll() {
	// Phase 3b：先把每张 bindless 纹理还给 pool，再 clear。调用方需保证 pool 仍存活
	// （GameApp::Shutdown 顺序：先 RM.UnloadAll → 再 pool->Shutdown → 再 ctx->Shutdown）。
	if (mTexturePool) {
		for (auto& kv : mTextures) {
			if (kv.second.vkTex) mTexturePool->DestroyTexture(kv.second.vkTex);
		}
	}
	mTextures.clear();
	mAtlasPages.clear();

	// 清理动画
	mReanimations.clear();

	// 清理字体
	for (auto& fontPair : fonts) {
		for (auto& sizePair : fontPair.second) {
			TTF_CloseFont(sizePair.second);
		}
	}
	fonts.clear();

	// 清理音效
	for (auto& pair : sounds) {
		Mix_FreeChunk(pair.second);
	}
	sounds.clear();

	// 清理音乐
	for (auto& pair : music) {
		Mix_FreeMusic(pair.second);
	}
	music.clear();

	LOG_DEBUG("ResourceManager") << "已卸载所有资源";
}