#include "ResourceManager.h"

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
    std::cout << "开始加载游戏图片资源..." << std::endl;

    for (const auto& path : gameImagePaths)
    {
        if (!LoadTexture(path))
        {
            std::cerr << "加载游戏图片失败: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "游戏图片资源加载完成，成功: "
        << textures.size() << "/" << gameImagePaths.size() << std::endl;
    return success;
}

bool ResourceManager::LoadAllParticleTextures()
{
    bool success = true;
    std::cout << "开始加载粒子纹理..." << std::endl;

    for (const auto& path : particleTexturePaths)
    {
        std::string key = "particle_" + std::to_string(particleTexturePaths.size());
        if (!LoadTexture(path, key))
        {
            std::cerr << "加载粒子纹理失败: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "粒子纹理加载完成" << std::endl;
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

    std::cout << "注册字体: " << path << " (key: " << actualKey << ")" << std::endl;
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

    std::cout << "需要按需加载大小 " << size << std::endl;

    // 找到字体文件路径 - 修复查找逻辑
    std::string fontPath;
    for (const auto& path : fontPaths)
    {
        // 从路径中提取文件名（不含扩展名）进行比较
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string fileKey = filename.substr(0, filename.find_last_of('.'));

        std::cout << "比较: path='" << path << "', fileKey='" << fileKey << "' with key='" << key << "'" << std::endl;

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
            std::cout << "尝试完整路径匹配: path='" << path << "' with key='" << key << "'" << std::endl;
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

    std::cout << "使用字体路径: " << fontPath << std::endl;

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
    std::cout << "开始注册字体..." << std::endl;

    for (const auto& path : fontPaths)
    {
        if (!LoadFont(path))
        {
            std::cerr << "注册字体失败: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "字体注册完成" << std::endl;
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
        std::cout << "卸载字体: " << key << std::endl;
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
            std::cout << "卸载字体: " << key << " size: " << size << std::endl;

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
    std::cout << "清理了 " << removedCount << " 个空字体条目" << std::endl;
}

bool ResourceManager::LoadAllSounds()
{
    bool success = true;
    std::cout << "开始加载音效..." << std::endl;

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

    std::cout << "音效加载完成" << std::endl;
    return success;
}

bool ResourceManager::LoadAllMusic()
{
    bool success = true;
    std::cout << "开始加载音乐..." << std::endl;

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

    std::cout << "音乐加载完成" << std::endl;
    return success;
}

bool ResourceManager::LoadAllAnimations()
{
    if (!renderer) {
        std::cerr << "错误: ResourceManager 未初始化，无法加载动画！" << std::endl;
        return false;
    }

    bool success = true;
    int loadedCount = 0;
    std::cout << "开始加载所有动画资源..." << std::endl;

    for (const auto& animPair : animationPaths) {
        AnimationType animType = animPair.first;
        const std::string& path = animPair.second;

        if (LoadAnimation(animType)) {
            loadedCount++;
            std::cout << "成功加载动画: " << path << " (类型: " << static_cast<int>(animType) << ")" << std::endl;
        }
        else {
            success = false;
            std::cerr << "加载动画失败: " << path << " (类型: " << static_cast<int>(animType) << ")" << std::endl;
        }
    }

    std::cout << "动画资源加载完成，成功: " << loadedCount << "/" << animationPaths.size() << std::endl;
    return success;
}

void ResourceManager::UnloadAnimation(AnimationType animType)
{
    auto it = animations.find(animType);
    if (it != animations.end()) {
        it->second->Clear();
        animations.erase(it);
        std::cout << "卸载动画: " << static_cast<int>(animType) << std::endl;
    }
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

std::shared_ptr<ReanimatorDefinition> ResourceManager::GetAnimation(AnimationType animType)
{
    auto it = animations.find(animType);
    if (it != animations.end()) {
        return it->second;
    }

    // 如果动画未加载，尝试立即加载
    std::cout << "动画类型 " << static_cast<int>(animType) << " 未加载，尝试立即加载..." << std::endl;
    if (LoadAnimation(animType)) {
        return animations[animType];
    }

    std::cerr << "错误: 无法获取动画类型 " << static_cast<int>(animType) << std::endl;
    return nullptr;
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
    std::cout << "成功加载纹理: " << path << " (key: " << actualKey << ")" << std::endl;
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

void ResourceManager::UnloadTexture(const std::string& key)
{
    auto it = textures.find(key);
    if (it != textures.end())
    {
        SDL_DestroyTexture(it->second);
        textures.erase(it);
        std::cout << "卸载纹理: " << key << std::endl;
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
    std::cout << "成功加载音效: " << path << std::endl;
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
        std::cout << "卸载音效: " << key << std::endl;
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
    std::cout << "成功加载音乐: " << path << std::endl;
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
        std::cout << "卸载音乐: " << key << std::endl;
    }
}

void ResourceManager::LoadTexturePack(const std::vector<std::pair<std::string, std::string>>& texturePaths)
{
    for (const auto& pair : texturePaths)
    {
        LoadTexture(pair.first, pair.second);
    }
}

bool ResourceManager::LoadAnimation(AnimationType animType)
{
    if (!renderer) {
        std::cerr << "错误: ResourceManager 未初始化！" << std::endl;
        return false;
    }

    if (animations.find(animType) != animations.end()) {
        return true;
    }

    auto pathIt = animationPaths.find(animType);
    if (pathIt == animationPaths.end()) {
        std::cerr << "错误: 未找到动画类型 " << static_cast<int>(animType) << " 的路径配置" << std::endl;
        return false;
    }

    const std::string& path = pathIt->second;

    auto definition = std::make_shared<ReanimatorDefinition>();
    if (!definition->LoadFromFile(path)) {
        std::cerr << "加载动画文件失败: " << path << std::endl;
        return false;
    }

    if (!definition->LoadImages(renderer)) {
        std::cerr << "警告: 动画图片加载失败或部分失败: " << path << std::endl;
    }

    animations[animType] = definition;
    return true;
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
    for (auto& pair : animations) 
    {
        pair.second->Clear();
    }
    animations.clear();

    std::cout << "已卸载所有资源" << std::endl;
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

bool ResourceManager::HasAnimation(AnimationType animType) const
{
    return animations.find(animType) != animations.end();
}