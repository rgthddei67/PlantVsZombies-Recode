#include "ResourceManager.h"
#include "./Game/Plant/GameDataManager.h"
#include <filesystem> 
#include <algorithm>
#include <cctype>   
#include <iostream>

namespace fs = std::filesystem;

ResourceManager* ResourceManager::instance = nullptr;

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

const GLTexture* ResourceManager::LoadTexture(const std::string& filepath, const std::string& key) {
    std::string actualKey = key.empty() ? filepath : key;
    auto it = mGLTextures.find(actualKey);
    if (it != mGLTextures.end()) {
        return &it->second;
    }

    // 使用 SDL_image 加载表面
    SDL_Surface* surface = IMG_Load(filepath.c_str());
    if (!surface) {
        std::cerr << "[ResourceManager::LoadTexture] 无法加载图片: " << filepath << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }

    // 统一转换为 RGBA，解决 GL_UNPACK_ALIGNMENT 对齐问题和格式不确定问题
    SDL_Surface* converted = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);
    if (!converted) {
        std::cerr << "[ResourceManager::LoadTexture] 无法转换图片格式: " << filepath << std::endl;
        return nullptr;
    }

    // 创建 OpenGL 纹理
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, converted->w, converted->h, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, converted->pixels);

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 保存纹理尺寸
    int w = converted->w;
    int h = converted->h;
    SDL_FreeSurface(converted);

    GLTexture tex;
    tex.id = textureId;
    tex.width = w;
    tex.height = h;
    mGLTextures[actualKey] = tex;

    //std::cout << "成功加载图片: " << filepath << " (key: " << actualKey
    //    << ") 尺寸 " << tex.width << "x" << tex.height << std::endl;
    return &mGLTextures[actualKey];
}

const GLTexture* ResourceManager::GetTexture(const std::string& key) const {
    auto it = mGLTextures.find(key);
    return (it != mGLTextures.end()) ? &it->second : nullptr;
}

void ResourceManager::UnloadTexture(const std::string& key) {
    auto it = mGLTextures.find(key);
    if (it != mGLTextures.end()) {
        glDeleteTextures(1, &it->second.id);
        mGLTextures.erase(it);
#ifdef _DEBUG
        std::cout << "卸载纹理: " << key << std::endl;
#endif
    }
}

bool ResourceManager::HasTexture(const std::string& key) const {
    return mGLTextures.find(key) != mGLTextures.end();
}

bool ResourceManager::LoadTiledTextureGL(const TiledImageInfo& info, const std::string& prefix) {
    // 加载原图到表面（用于分割）
    SDL_Surface* originalSurface = IMG_Load(info.path.c_str());
    if (!originalSurface) {
        std::cerr << "无法加载图片: " << info.path << " - " << IMG_GetError() << std::endl;
        return false;
    }

    int imgW = originalSurface->w;
    int imgH = originalSurface->h;
    int tileW = imgW / info.columns;
    int tileH = imgH / info.rows;

    if (imgW % info.columns != 0 || imgH % info.rows != 0) {
        std::cerr << "警告: 图片尺寸 " << imgW << "x" << imgH
            << " 不能被 " << info.columns << "x" << info.rows
            << " 整除，可能产生边缘裁剪" << std::endl;
    }

    std::string baseKey = GenerateStandardKey(info.path, prefix);
    bool success = true;

    for (int row = 0; row < info.rows; ++row) {
        for (int col = 0; col < info.columns; ++col) {
            // 创建子表面
            SDL_Surface* tileSurface = SDL_CreateRGBSurface(
                0, tileW, tileH,
                originalSurface->format->BitsPerPixel,
                originalSurface->format->Rmask,
                originalSurface->format->Gmask,
                originalSurface->format->Bmask,
                originalSurface->format->Amask
            );
            if (!tileSurface) {
                std::cerr << "无法创建子表面: " << SDL_GetError() << std::endl;
                success = false;
                continue;
            }

            SDL_Rect srcRect = { col * tileW, row * tileH, tileW, tileH };
            if (SDL_BlitSurface(originalSurface, &srcRect, tileSurface, nullptr) != 0) {
                std::cerr << "Blit失败: " << SDL_GetError() << std::endl;
                SDL_FreeSurface(tileSurface);
                success = false;
                continue;
            }

            // 统一转换为 RGBA，解决对齐和格式问题
            SDL_Surface* tileConverted = SDL_ConvertSurfaceFormat(tileSurface, SDL_PIXELFORMAT_ABGR8888, 0);
            SDL_FreeSurface(tileSurface);
            if (!tileConverted) {
                std::cerr << "无法转换子图片格式: " << SDL_GetError() << std::endl;
                success = false;
                continue;
            }

            // 创建 OpenGL 纹理
            GLuint textureId;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);

            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, tileW, tileH, 0,
                GL_RGBA, GL_UNSIGNED_BYTE, tileConverted->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            SDL_FreeSurface(tileConverted);

            GLTexture tex;
            tex.id = textureId;
            tex.width = tileW;
            tex.height = tileH;
            int index = row * info.columns + col;
            std::string key = baseKey + "_PART_" + std::to_string(index);
            mGLTextures[key] = tex;

            // std::cout << "加载子纹理: " << key << " 尺寸 " << tileW << "x" << tileH << std::endl;
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
                std::cerr << "加载游戏图片失败: " << info.path << std::endl;
                success = false;
            }
        }
        else {
            // 分割贴图
            if (!LoadTiledTextureGL(info, "IMAGE_")) {
                std::cerr << "加载分割贴图失败: " << info.path << std::endl;
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
                std::cerr << "加载粒子纹理失败: " << info.path << std::endl;
                success = false;
            }
        }
        else {
            if (!LoadTiledTextureGL(info, "PARTICLE_")) {
                std::cerr << "加载粒子分割贴图失败: " << info.path << std::endl;
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
            std::cerr << "注册字体失败: " << path << std::endl;
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
            std::cerr << "加载音效失败: " << path << std::endl;
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
            std::cerr << "加载音乐失败: " << path << std::endl;
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
            std::cerr << "加载动画失败: " << key << " from " << path << std::endl;
        }
    }
    return success;
}

bool ResourceManager::LoadAllImagesFromPath(const std::string& directory) {
    bool allSuccess = true;
    int loadedCount = 0;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        std::cerr << "目录不存在: " << directory << std::endl;
        return false;
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) continue;

        fs::path filepath = entry.path();
        std::string ext = filepath.extension().string();
        // 转换为小写以统一判断
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        if (ext == ".jpg" || ext == ".jpeg" || ext == ".png") {
            std::string key = filepath.stem().string();  // 不带扩展名的文件名
            // 将 key 转换为大写
            std::transform(key.begin(), key.end(), key.begin(), ::toupper);
            // 添加前缀 "IMAGE_"
            key = "IMAGE_" + key;
            std::string fullPath = filepath.string();

            if (LoadTexture(fullPath, key)) {
                loadedCount++;
            }
            else {
                allSuccess = false;
            }
        }
    }

    if (loadedCount == 0) {
        std::cerr << "在目录 " << directory << " 中没有找到任何 JPG/PNG 图片" << std::endl;
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
        std::cerr << "Failed to load reanimation: " << path << std::endl;
        return nullptr;
    }

    mReanimations[key] = reanim;
//#ifdef _DEBUG
//    std::cout << "Successfully loaded reanimation: " << key << " from " << path << std::endl;
//#endif
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
        newReanim->mTracks = cachedReanim->mTracks;  // 假设 mTracks 可复制
        return newReanim;
    }
    std::cerr << "Reanimation not found: " << key << std::endl;
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
//#ifdef _DEBUG
//    std::cout << "注册字体: " << path << " (key: " << actualKey << ")" << std::endl;
//#endif
    return true;
}

TTF_Font* ResourceManager::GetFont(const std::string& key, int size) {
    if (key.empty() || size <= 0) {
        std::cerr << "无效的字体参数: key=" << key << ", size=" << size << std::endl;
        return nullptr;
    }

    auto fontIt = fonts.find(key);
    if (fontIt == fonts.end()) {
        std::cerr << "字体未注册: '" << key << "'" << std::endl;
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

    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), size);
    if (!font) {
        std::cerr << "加载字体失败: " << fontPath << " size: " << size << " - " << TTF_GetError() << std::endl;
        return nullptr;
    }

    sizeMap[size] = font;
    //std::cout << "成功加载字体: '" << key << "' size: " << size << std::endl;
    return font;
}

void ResourceManager::UnloadFont(const std::string& key) {
    auto it = fonts.find(key);
    if (it != fonts.end()) {
        for (auto& sizePair : it->second) {
            TTF_CloseFont(sizePair.second);
        }
        fonts.erase(it);
#ifdef _DEBUG
        std::cout << "卸载字体: " << key << std::endl;
#endif
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
#ifdef _DEBUG
            std::cout << "卸载字体: " << key << " size: " << size << std::endl;
#endif
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

    Mix_Chunk* sound = Mix_LoadWAV(path.c_str());
    if (!sound) {
        std::cerr << "加载音效失败: " << path << " - " << Mix_GetError() << std::endl;
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
    std::cerr << "警告: 音效未找到: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadSound(const std::string& key) {
    auto it = sounds.find(key);
    if (it != sounds.end()) {
        Mix_FreeChunk(it->second);
        sounds.erase(it);
#ifdef _DEBUG
        std::cout << "卸载音效: " << key << std::endl;
#endif
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

    Mix_Music* mus = Mix_LoadMUS(path.c_str());
    if (!mus) {
        std::cerr << "加载音乐失败: " << path << " - " << Mix_GetError() << std::endl;
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
    std::cerr << "警告: 音乐未找到: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadMusic(const std::string& key) {
    auto it = music.find(key);
    if (it != music.end()) {
        Mix_FreeMusic(it->second);
        music.erase(it);
#ifdef _DEBUG
        std::cout << "卸载音乐: " << key << std::endl;
#endif
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
    // 清理 OpenGL 纹理
    for (auto& pair : mGLTextures) {
        glDeleteTextures(1, &pair.second.id);
    }
    mGLTextures.clear();

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

#ifdef _DEBUG
    std::cout << "已卸载所有资源" << std::endl;
#endif
}