#include "ResourceManager.h"
#include "./Game/Plant/GameDataManager.h"
#include <algorithm>
#include <iostream>

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
        std::cerr << "无法加载图片: " << filepath << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }

    // 创建 OpenGL 纹理
    GLuint textureId;
    glGenTextures(1, &textureId);
    glBindTexture(GL_TEXTURE_2D, textureId);

    // 判断像素格式
    GLenum format = (surface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
    glTexImage2D(GL_TEXTURE_2D, 0, format, surface->w, surface->h, 0,
        format, GL_UNSIGNED_BYTE, surface->pixels);

    // 设置纹理参数
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 保存纹理尺寸
    int w = surface->w;
    int h = surface->h;
    SDL_FreeSurface(surface);

    GLTexture tex;
    tex.id = textureId;
    tex.width = w;
    tex.height = h;
    mGLTextures[actualKey] = tex;

#ifdef _DEBUG
    std::cout << "成功加载 OpenGL 纹理: " << filepath << " (key: " << actualKey
        << ") 尺寸 " << tex.width << "x" << tex.height << std::endl;
#endif
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

            // 创建 OpenGL 纹理
            GLuint textureId;
            glGenTextures(1, &textureId);
            glBindTexture(GL_TEXTURE_2D, textureId);

            GLenum format = (tileSurface->format->BytesPerPixel == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_2D, 0, format, tileW, tileH, 0,
                format, GL_UNSIGNED_BYTE, tileSurface->pixels);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            SDL_FreeSurface(tileSurface);

            GLTexture tex;
            tex.id = textureId;
            tex.width = tileW;
            tex.height = tileH;
            int index = row * info.columns + col;
            std::string key = baseKey + "_PART_" + std::to_string(index);
            mGLTextures[key] = tex;

#ifdef _DEBUG
            std::cout << "加载子纹理: " << key << " 尺寸 " << tileW << "x" << tileH << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "开始注册字体..." << std::endl;
#endif
    const auto& paths = configReader.GetFontPaths();
    for (const auto& path : paths)
    {
        if (!LoadFont(path))
        {
            std::cerr << "注册字体失败: " << path << std::endl;
            success = false;
        }
    }
#ifdef _DEBUG
    std::cout << "字体注册完成" << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadAllSounds()
{
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载音效..." << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "音效加载完成" << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadAllMusic()
{
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载音乐..." << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "音乐加载完成" << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadAllReanimations()
{
    bool success = true;
    int loadedCount = 0;
#ifdef _DEBUG
    std::cout << "开始加载所有动画资源..." << std::endl;
#endif
    const auto& reanimPaths = configReader.GetReanimationPaths();
    for (const auto& reanimPair : reanimPaths) {
        const std::string& key = reanimPair.first;
        const std::string& path = reanimPair.second;

        if (LoadReanimation(key, path)) {
            loadedCount++;
            std::cout << "成功加载动画: " << key << " from " << path << std::endl;
        }
        else {
            success = false;
            std::cerr << "加载动画失败: " << key << " from " << path << std::endl;
        }
    }
#ifdef _DEBUG
    std::cout << "动画资源加载完成，成功: " << loadedCount << "/" << reanimPaths.size() << std::endl;
#endif
    return success;
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
#ifdef _DEBUG
    std::cout << "Successfully loaded reanimation: " << key << " from " << path << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "注册字体: " << path << " (key: " << actualKey << ")" << std::endl;
#endif
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
    std::cout << "成功加载字体: '" << key << "' size: " << size << std::endl;
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
#ifdef _DEBUG
    std::cout << "成功加载音效: " << path << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "成功加载音乐: " << path << std::endl;
#endif
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