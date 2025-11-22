#include "ResourceManager.h"
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

bool ResourceManager::LoadAllGameImages()
{
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载游戏图片资源..." << std::endl;
#endif
    for (const auto& path : gameImagePaths)
    {
        std::string key = GenerateTextureKey(path);
        if (!LoadTexture(path, key))
        {
            std::cerr << "加载游戏图片失败: " << path << std::endl;
            success = false;
        }
    }
#ifdef _DEBUG
    std::cout << "游戏图片资源加载完成，成功: "
        << textures.size() << "/" << gameImagePaths.size() << std::endl;
#endif
    return success;
}

bool ResourceManager::LoadAllParticleTextures()
{
    bool success = true;
#ifdef _DEBUG
    std::cout << "开始加载粒子纹理..." << std::endl;
#endif
    for (const auto& path : particleTexturePaths)
    {
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string nameWithoutExt = filename.substr(0, filename.find_last_of('.'));

        // 转换为小写
        std::transform(nameWithoutExt.begin(), nameWithoutExt.end(), nameWithoutExt.begin(), ::tolower);

        // 生成key：particle_ + 文件名（小写）
        std::string key = "particle_" + nameWithoutExt;
        if (!LoadTexture(path, key))
        {
            std::cerr << "加载粒子纹理失败: " << path << std::endl;
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

    //std::cout << "调试: 查找字体 key='" << key << "', size=" << size << std::endl;

    auto fontIt = fonts.find(key);
    if (fontIt == fonts.end())
    {
        std::cerr << "字体未注册: '" << key << "'" << std::endl;

        // 尝试查找可能的大小写或空格问题
        for (const auto& pair : fonts) 
        {
            if (pair.first.find(key) != std::string::npos ||
                key.find(pair.first) != std::string::npos) {
                //std::cout << "发现相似键: '" << pair.first << "'" << std::endl;
            }
        }
        return nullptr;
    }

    //std::cout << "找到字体键: '" << key << "'" << std::endl;

    // 检查该大小是否已加载
    auto& sizeMap = fontIt->second;
    auto sizeIt = sizeMap.find(size);
    if (sizeIt != sizeMap.end())
    {
        //std::cout << "字体大小 " << size << " 已加载" << std::endl;
        return sizeIt->second;
    }
#ifdef _DEBUG
    std::cout << "需要按需加载大小 " << size << std::endl;
#endif
    // 找到字体文件路径 - 修复查找逻辑
    std::string fontPath;
    for (const auto& path : fontPaths)
    {
        // 从路径中提取文件名（不含扩展名）进行比较
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string fileKey = filename.substr(0, filename.find_last_of('.'));
#ifdef _DEBUG
        std::cout << "比较: path='" << path << "', fileKey='" << fileKey << "' with key='" << key << "'" << std::endl;
#endif
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
#ifdef _DEBUG
    std::cout << "使用字体路径: " << fontPath << std::endl;
#endif
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
    for (const auto& path : fontPaths)
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
        total += fontPair.second.size();
    }
    return total;
}

// 卸载特定字体大小的方法
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
    for (const auto& path : soundPaths)
    {
        std::string key = "sound_" + path.substr(path.find_last_of("/\\") + 1);
        key = key.substr(0, key.find_last_of('.'));
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
    for (const auto& path : musicPaths)
    {
        std::string key = "music_" + path.substr(path.find_last_of("/\\") + 1);
        key = key.substr(0, key.find_last_of('.'));
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
        if (cachedReanim->mTracks) {
            newReanim->mTracks = std::make_shared<std::vector<TrackInfo>>(*cachedReanim->mTracks);
        }
        if (cachedReanim->mImagesSet) {
            newReanim->mImagesSet = std::make_shared<std::set<std::string>>(*cachedReanim->mImagesSet);
        }

        return newReanim;
    }

    std::cerr << "Reanimation not found: " << key << std::endl;
    return nullptr;
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
    for (const auto& reanimPair : mReanimationPaths) {
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
    std::cout << "动画资源加载完成，成功: " << loadedCount << "/" << mReanimationPaths.size() << std::endl;
#endif
    return success;
}

// 获取资源路径列表
const std::vector<std::string>& ResourceManager::GetGameImagePaths() const
{
    return gameImagePaths;
}

const std::vector<std::string>& ResourceManager::GetParticleTexturePaths() const
{
    return particleTexturePaths;
}

const std::vector<std::string>& ResourceManager::GetSoundPaths() const
{
    return soundPaths;
}

const std::vector<std::string>& ResourceManager::GetMusicPaths() const
{
    return musicPaths;
}

void ResourceManager::Initialize(SDL_Renderer* renderer)
{
    this->renderer = renderer;
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

    // 加载纹理
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
    if (!texture)
    {
        std::cerr << "加载纹理失败: " << path << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }

    textures[actualKey] = texture;
#ifdef _DEBUG
    std::cout << "成功加载纹理: " << path << " (key: " << actualKey << ")" << std::endl;
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
    return "IMAGE_" + nameWithoutExt;
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
