#include "ResourceManager.h"
#include "./Game/Plant/GameDataManager.h"
#include <algorithm>

ResourceManager* ResourceManager::instance = nullptr;

ResourceManager& ResourceManager::GetInstance()
{
    if (!instance)
    {
        instance = new ResourceManager();
    }
    return *instance;
}

void ResourceManager::ReleaseInstance()
{
    if (instance)
    {
        instance->UnloadAll();
        delete instance;
        instance = nullptr;
    }
}

bool ResourceManager::Initialize(SDL_Renderer* renderer, const std::string& configPath)
{
    this->renderer = renderer;

    if (!configReader.LoadConfig(configPath)) {
        std::cerr << "Failed to load resource configuration from: " << configPath << std::endl;
        return false;
    }

    return true;
}

bool ResourceManager::LoadAllGameImages() {
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载游戏图片资源..." << std::endl;
#endif
    const auto& infos = GetGameImageInfos();
    for (const auto& info : infos) {
        if (!LoadTiledTexture(info, "IMAGE_")) {
            std::cerr << "加载游戏图片失败: " << info.path << std::endl;
            success = false;
        }
    }
#ifdef _DEBUG
    std::cout << "游戏图片资源加载完成，成功: "
        << textures.size() << "/" << infos.size() << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadAllParticleTextures() {
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载粒子纹理..." << std::endl;
#endif
    const auto& infos = GetParticleTextureInfos();
    for (const auto& info : infos) {
        if (!LoadTiledTexture(info, "PARTICLE_")) {
            std::cerr << "加载粒子纹理失败: " << info.path << std::endl;
            success = false;
        }
    }
#ifdef _DEBUG
    std::cout << "粒子纹理加载完成" << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadFont(const std::string& path, const std::string& key)
{
    std::string actualKey = key.empty() ? path : key;

    // 检查是否已加载
    if (fonts.find(actualKey) != fonts.end())
    {
        return true; // 字体已加载
    }

    // 初始化空的字体大小映射
    fonts[actualKey] = std::unordered_map<int, TTF_Font*>();
#ifdef _DEBUG
    std::cout << "注册字体: " << path << " (key: " << actualKey << ")" << std::endl;
#endif
    return true;
}

// 获取带大小字体
TTF_Font* ResourceManager::GetFont(const std::string& key, int size)
{
    if (key.empty() || size <= 0)
    {
        std::cerr << "无效的字体参数: key=" << key << ", size=" << size << std::endl;
        return nullptr;
    }

    auto fontIt = fonts.find(key);
    if (fontIt == fonts.end())
    {
        std::cerr << "字体未注册: '" << key << "'" << std::endl;
        return nullptr;
    }

    // 检查该大小是否已加载
    auto& sizeMap = fontIt->second;
    auto sizeIt = sizeMap.find(size);
    if (sizeIt != sizeMap.end())
    {
        return sizeIt->second;
    }
    // 找到字体文件路径
    std::string fontPath;
    const auto& fontPaths = configReader.GetFontPaths();
    for (const auto& path : fontPaths)
    {
        // 从路径中提取文件名（不含扩展名）进行比较
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string fileKey = filename.substr(0, filename.find_last_of('.'));
        if (fileKey == key)
        {
            fontPath = path;
            break;
        }
    }

    if (fontPath.empty())
    {
        // 如果没找到，尝试使用完整路径作为键名
        for (const auto& path : fontPaths)
        {
#ifdef _DEBUG
            std::cout << "尝试完整路径匹配: path='" << path << "' with key='" << key << "'" << std::endl;
#endif
            if (path == key) // 直接比较完整路径
            {
                fontPath = path;
                break;
            }
        }
    }

    if (fontPath.empty())
    {
        std::cerr << "错误: 找不到字体文件路径 for key: '" << key << "'" << std::endl;
        std::cout << "可用的字体路径: ";
        for (const auto& path : fontPaths) {
            std::cout << path << " ";
        }
        std::cout << std::endl;
        return nullptr;
    }
    // 按需加载特定大小的字体
    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), size);
    if (!font)
    {
        std::cerr << "加载字体失败: " << fontPath << " size: " << size << " - " << TTF_GetError() << std::endl;
        return nullptr;
    }

    sizeMap[size] = font;
    std::cout << "成功加载字体: '" << key << "' size: " << size << std::endl;
    return font;
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

void ResourceManager::UnloadFont(const std::string& key)
{
    auto it = fonts.find(key);
    if (it != fonts.end())
    {
        for (auto& sizePair : it->second)
        {
            TTF_CloseFont(sizePair.second);
        }
        fonts.erase(it);
#ifdef _DEBUG
        std::cout << "卸载字体: " << key << std::endl;
#endif
    }
}

std::vector<int> ResourceManager::GetLoadedFontSizes(const std::string& key) const
{
    std::vector<int> sizes;
    auto it = fonts.find(key);
    if (it != fonts.end())
    {
        for (const auto& sizePair : it->second)
        {
            sizes.push_back(sizePair.first);
        }
    }
    return sizes;
}

int ResourceManager::GetLoadedFontCount() const
{
    int total = 0;
    for (const auto& fontPair : fonts)
    {
        total += static_cast<int>(fontPair.second.size());
    }
    return total;
}

// 卸载特定字体大小
void ResourceManager::UnloadFontSize(const std::string& key, int size)
{
    auto fontIt = fonts.find(key);
    if (fontIt != fonts.end())
    {
        auto& sizeMap = fontIt->second;
        auto sizeIt = sizeMap.find(size);
        if (sizeIt != sizeMap.end())
        {
            TTF_CloseFont(sizeIt->second);
            sizeMap.erase(sizeIt);
#ifdef _DEBUG
            std::cout << "卸载字体: " << key << " size: " << size << std::endl;
#endif
            // 如果这个字体没有其他大小了，完全移除
            if (sizeMap.empty())
            {
                fonts.erase(fontIt);
            }
        }
    }
}

// 清理未使用字体大小
void ResourceManager::CleanupUnusedFontSizes()
{
    int removedCount = 0;
    for (auto it = fonts.begin(); it != fonts.end(); )
    {
        if (it->second.empty())
        {
            it = fonts.erase(it);
            removedCount++;
        }
        else
        {
            ++it;
        }
    }
#ifdef _DEBUG
    std::cout << "清理了 " << removedCount << " 个空字体条目" << std::endl;
#endif
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

std::shared_ptr<Reanimation> ResourceManager::LoadReanimation(const std::string& key, const std::string& path) {
    // 检查是否已加载
    auto it = mReanimations.find(key);
    if (it != mReanimations.end()) {
        return it->second;
    }

    // 创建新的Reanimation
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
        // 从缓存获取数据，并创建新的独立实例
        auto cachedReanim = it->second;
        auto newReanim = std::make_shared<Reanimation>();

        // 复制基础属性
        newReanim->mFPS = cachedReanim->mFPS;
        newReanim->mIsLoaded = cachedReanim->mIsLoaded;
        newReanim->mResourceManager = cachedReanim->mResourceManager;

        // 深拷贝轨道数据
        newReanim->mTracks = cachedReanim->mTracks;

        return newReanim;
    }

    std::cerr << "Reanimation not found: " << key << std::endl;
    return nullptr;
}

std::string ResourceManager::AnimationTypeToString(AnimationType type) {
    return GameDataManager::GetInstance().GetAnimationName(type);
}

void ResourceManager::UnloadReanimation(const std::string& key) {
    auto it = mReanimations.find(key);
    if (it != mReanimations.end()) {
        mReanimations.erase(it);
#ifdef _DEBUG
        std::cout << "Unloaded reanimation: " << key << std::endl;
#endif
    }
}

bool ResourceManager::HasReanimation(const std::string& key) const {
    return mReanimations.find(key) != mReanimations.end();
}

bool ResourceManager::LoadAllReanimations()
{
    if (!renderer) {
        std::cerr << "错误: ResourceManager 未初始化，无法加载动画！" << std::endl;
        return false;
    }

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

SDL_Texture* ResourceManager::LoadTexture(const std::string& path, const std::string& key)
{
    if (!renderer)
    {
        std::cerr << "错误: ResourceManager 未初始化！" << std::endl;
        return nullptr;
    }

    std::string actualKey = key.empty() ? path : key;

    // 检查是否已加载
    if (textures.find(actualKey) != textures.end())
    {
        return textures[actualKey];
    }

    SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
    if (!texture)
    {
        // 尝试不同的路径格式
        std::cerr << "加载纹理失败: " << path << " - " << IMG_GetError() << std::endl;

        // 检查文件是否存在
        SDL_RWops* file = SDL_RWFromFile(path.c_str(), "rb");
        if (!file) {
            std::cerr << "文件不存在或无法打开: " << path << std::endl;
        }
        else {
            SDL_RWclose(file);
        }

        return nullptr;
    }

    // 获取纹理尺寸并验证
    int width, height;
    if (SDL_QueryTexture(texture, NULL, NULL, &width, &height) != 0) {
        std::cerr << "获取纹理尺寸失败: " << path << " - " << SDL_GetError() << std::endl;
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    if (width <= 0 || height <= 0) {
        std::cerr << "无效的纹理尺寸: " << width << "x" << height << " for " << path << std::endl;
        SDL_DestroyTexture(texture);
        return nullptr;
    }

    // 设置纹理属性
    SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
    SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");

    textures[actualKey] = texture;

#ifdef _DEBUG
    std::cout << "成功加载纹理: " << path << " (key: " << actualKey << ") 尺寸: "
        << width << "x" << height << std::endl;
#endif
    return texture;
}

SDL_Texture* ResourceManager::GetTexture(const std::string& key)
{
    auto it = textures.find(key);
    if (it != textures.end())
    {
        return it->second;
    }
    std::cerr << "警告: 纹理未找到: " << key << std::endl;
    return nullptr;
}

std::string ResourceManager::GenerateTextureKey(const std::string& path)
{
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    std::string nameWithoutExt = filename.substr(0, filename.find_last_of('.'));

    // 转换为大写
    std::transform(nameWithoutExt.begin(), nameWithoutExt.end(), nameWithoutExt.begin(), ::toupper);

    // 将非字母数字字符替换为下划线
    for (char& c : nameWithoutExt) {
        if (!std::isalnum(c)) {
            c = '_';
        }
    }

    return "IMAGE_" + nameWithoutExt;
}

std::string ResourceManager::GenerateStandardKey(const std::string& path, const std::string& prefix)
{
    std::string filename = path.substr(path.find_last_of("/\\") + 1);
    std::string nameWithoutExt = filename.substr(0, filename.find_last_of('.'));

    // 转换为大写
    std::transform(nameWithoutExt.begin(), nameWithoutExt.end(), nameWithoutExt.begin(), ::toupper);

    // 将非字母数字字符替换为下划线
    for (char& c : nameWithoutExt) {
        if (!std::isalnum(c)) {
            c = '_';
        }
    }

    return prefix + nameWithoutExt;
}

void ResourceManager::UnloadTexture(const std::string& key)
{
    auto it = textures.find(key);
    if (it != textures.end())
    {
        SDL_DestroyTexture(it->second);
        textures.erase(it);
#ifdef _DEBUG
        std::cout << "卸载纹理: " << key << std::endl;
#endif
    }
}

Mix_Chunk* ResourceManager::LoadSound(const std::string& path, const std::string& key)
{
    std::string actualKey = key.empty() ? path : key;

    if (sounds.find(actualKey) != sounds.end())
    {
        return sounds[actualKey];
    }

    Mix_Chunk* sound = Mix_LoadWAV(path.c_str());
    if (!sound)
    {
        std::cerr << "加载音效失败: " << path << " - " << Mix_GetError() << std::endl;
        return nullptr;
    }

    sounds[actualKey] = sound;
#ifdef _DEBUG
    std::cout << "成功加载音效: " << path << std::endl;
#endif
    return sound;
}

Mix_Chunk* ResourceManager::GetSound(const std::string& key)
{
    auto it = sounds.find(key);
    if (it != sounds.end())
    {
        return it->second;
    }
    std::cerr << "警告: 音效未找到: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadSound(const std::string& key)
{
    auto it = sounds.find(key);
    if (it != sounds.end())
    {
        Mix_FreeChunk(it->second);
        sounds.erase(it);
#ifdef _DEBUG
        std::cout << "卸载音效: " << key << std::endl;
#endif
    }
}

Mix_Music* ResourceManager::LoadMusic(const std::string& path, const std::string& key)
{
    std::string actualKey = key.empty() ? path : key;

    if (music.find(actualKey) != music.end())
    {
        return music[actualKey];
    }

    Mix_Music* music = Mix_LoadMUS(path.c_str());
    if (!music)
    {
        std::cerr << "加载音乐失败: " << path << " - " << Mix_GetError() << std::endl;
        return nullptr;
    }

    this->music[actualKey] = music;
#ifdef _DEBUG
    std::cout << "成功加载音乐: " << path << std::endl;
#endif
    return music;
}

Mix_Music* ResourceManager::GetMusic(const std::string& key)
{
    auto it = music.find(key);
    if (it != music.end())
    {
        return it->second;
    }
    std::cerr << "警告: 音乐未找到: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadMusic(const std::string& key)
{
    auto it = music.find(key);
    if (it != music.end())
    {
        Mix_FreeMusic(it->second);
        music.erase(it);
#ifdef _DEBUG
        std::cout << "卸载音乐: " << key << std::endl;
#endif
    }
}

void ResourceManager::LoadTexturePack(const std::vector<std::pair<std::string, std::string>>& texturePaths)
{
    for (const auto& pair : texturePaths)
    {
        LoadTexture(pair.first, pair.second);
    }
}

SDL_Texture* ResourceManager::CreateTextureFromSurface(SDL_Surface* surface) {
    if (!surface || !renderer) return nullptr;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    if (texture) {
        SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
    }
    return texture;
}

// 加载分割贴图
bool ResourceManager::LoadTiledTexture(const TiledImageInfo& info, const std::string& prefix) {
    // 如果不需要分割，直接调用 LoadTexture
    if (info.columns <= 1 && info.rows <= 1) {
        std::string key = GenerateStandardKey(info.path, prefix);
        return LoadTexture(info.path, key) != nullptr;
    }

    // 加载原图到Surface
    SDL_Surface* originalSurface = IMG_Load(info.path.c_str());
    if (!originalSurface) {
        std::cerr << "无法加载图片: " << info.path << " - " << IMG_GetError() << std::endl;
        return false;
    }

    int imgW = originalSurface->w;
    int imgH = originalSurface->h;
    int tileW = imgW / info.columns;
    int tileH = imgH / info.rows;

    // 检查是否整除
    if (imgW % info.columns != 0 || imgH % info.rows != 0) {
        std::cerr << "警告: 图片尺寸 " << imgW << "x" << imgH
            << " 不能被 " << info.columns << "x" << info.rows
            << " 整除，可能产生边缘裁剪" << std::endl;
    }

    std::string baseKey = GenerateStandardKey(info.path, prefix);
    bool success = true;

    // 遍历每个子图
    for (int row = 0; row < info.rows; ++row) {
        for (int col = 0; col < info.columns; ++col) {
            // 创建子Surface
            SDL_Surface* tileSurface = SDL_CreateRGBSurface(
                0, tileW, tileH,
                originalSurface->format->BitsPerPixel,
                originalSurface->format->Rmask,
                originalSurface->format->Gmask,
                originalSurface->format->Bmask,
                originalSurface->format->Amask
            );
            if (!tileSurface) {
                std::cerr << "无法创建子Surface: " << SDL_GetError() << std::endl;
                success = false;
                continue;
            }

            // 设置源矩形
            SDL_Rect srcRect = { col * tileW, row * tileH, tileW, tileH };
            // 将原图指定区域复制到子Surface
            if (SDL_BlitSurface(originalSurface, &srcRect, tileSurface, nullptr) != 0) {
                std::cerr << "Blit失败: " << SDL_GetError() << std::endl;
                SDL_FreeSurface(tileSurface);
                success = false;
                continue;
            }

            // 创建纹理
            SDL_Texture* texture = CreateTextureFromSurface(tileSurface);
            SDL_FreeSurface(tileSurface);

            if (!texture) {
                std::cerr << "无法从子Surface创建纹理" << std::endl;
                success = false;
                continue;
            }

            // 生成带索引的key
            int index = row * info.columns + col;
            std::string key = baseKey + "_PART_" + std::to_string(index);
            textures[key] = texture;
#ifdef _DEBUG
            std::cout << "加载子纹理: " << key << " 尺寸 " << tileW << "x" << tileH << std::endl;
#endif
        }
    }

    SDL_FreeSurface(originalSurface);
    return success;
}

void ResourceManager::UnloadAll()
{
    // 卸载所有纹理
    for (auto& pair : textures)
    {
        SDL_DestroyTexture(pair.second);
    }
    textures.clear();

    // 卸载所有字体
    for (auto& fontPair : fonts)
    {
        for (auto& sizePair : fontPair.second)
        {
            TTF_CloseFont(sizePair.second);
        }
    }
    fonts.clear();

    // 卸载所有音效
    for (auto& pair : sounds)
    {
        Mix_FreeChunk(pair.second);
    }
    sounds.clear();

    // 卸载所有音乐
    for (auto& pair : music)
    {
        Mix_FreeMusic(pair.second);
    }
    music.clear();

    // 卸载所有动画
    mReanimations.clear();
#ifdef _DEBUG
    std::cout << "已卸载所有资源" << std::endl;
#endif
}



bool ResourceManager::HasTexture(const std::string& key) const
{
    return textures.find(key) != textures.end();
}

bool ResourceManager::HasFont(const std::string& key) const
{
    return fonts.find(key) != fonts.end();
}

bool ResourceManager::HasSound(const std::string& key) const
{
    return sounds.find(key) != sounds.end();
}

bool ResourceManager::HasMusic(const std::string& key) const
{
    return music.find(key) != music.end();
}