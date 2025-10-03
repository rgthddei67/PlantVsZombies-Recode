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
    std::cout << "��ʼ������ϷͼƬ��Դ..." << std::endl;

    for (const auto& path : gameImagePaths)
    {
        if (!LoadTexture(path))
        {
            std::cerr << "������ϷͼƬʧ��: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "��ϷͼƬ��Դ������ɣ��ɹ�: "
        << textures.size() << "/" << gameImagePaths.size() << std::endl;
    return success;
}

bool ResourceManager::LoadAllParticleTextures()
{
    bool success = true;
    std::cout << "��ʼ������������..." << std::endl;

    for (const auto& path : particleTexturePaths)
    {
        std::string key = "particle_" + std::to_string(particleTexturePaths.size());
        if (!LoadTexture(path, key))
        {
            std::cerr << "������������ʧ��: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "��������������" << std::endl;
    return success;
}

bool ResourceManager::LoadFont(const std::string& path, const std::string& key)
{
    std::string actualKey = key.empty() ? path : key;

    // ����Ƿ��Ѽ���
    if (fonts.find(actualKey) != fonts.end())
    {
        return true; // �����Ѽ���
    }

    // ��ʼ���յ������Сӳ��
    fonts[actualKey] = std::unordered_map<int, TTF_Font*>();

    std::cout << "ע������: " << path << " (key: " << actualKey << ")" << std::endl;
    return true;
}

// ��ȡ����С����
TTF_Font* ResourceManager::GetFont(const std::string& key, int size)
{
    if (key.empty() || size <= 0)
    {
        std::cerr << "��Ч���������: key=" << key << ", size=" << size << std::endl;
        return nullptr;
    }

    //std::cout << "����: �������� key='" << key << "', size=" << size << std::endl;

    auto fontIt = fonts.find(key);
    if (fontIt == fonts.end())
    {
        std::cerr << "����δע��: '" << key << "'" << std::endl;

        // ���Բ��ҿ��ܵĴ�Сд��ո�����
        for (const auto& pair : fonts) 
        {
            if (pair.first.find(key) != std::string::npos ||
                key.find(pair.first) != std::string::npos) {
                //std::cout << "�������Ƽ�: '" << pair.first << "'" << std::endl;
            }
        }
        return nullptr;
    }

    //std::cout << "�ҵ������: '" << key << "'" << std::endl;

    // ���ô�С�Ƿ��Ѽ���
    auto& sizeMap = fontIt->second;
    auto sizeIt = sizeMap.find(size);
    if (sizeIt != sizeMap.end())
    {
        //std::cout << "�����С " << size << " �Ѽ���" << std::endl;
        return sizeIt->second;
    }

    std::cout << "��Ҫ������ش�С " << size << std::endl;

    // �ҵ������ļ�·�� - �޸������߼�
    std::string fontPath;
    for (const auto& path : fontPaths)
    {
        // ��·������ȡ�ļ�����������չ�������бȽ�
        std::string filename = path.substr(path.find_last_of("/\\") + 1);
        std::string fileKey = filename.substr(0, filename.find_last_of('.'));

        std::cout << "�Ƚ�: path='" << path << "', fileKey='" << fileKey << "' with key='" << key << "'" << std::endl;

        if (fileKey == key)
        {
            fontPath = path;
            break;
        }
    }

    if (fontPath.empty())
    {
        // ���û�ҵ�������ʹ������·����Ϊ����
        for (const auto& path : fontPaths)
        {
            std::cout << "��������·��ƥ��: path='" << path << "' with key='" << key << "'" << std::endl;
            if (path == key) // ֱ�ӱȽ�����·��
            {
                fontPath = path;
                break;
            }
        }
    }

    if (fontPath.empty())
    {
        std::cerr << "����: �Ҳ��������ļ�·�� for key: '" << key << "'" << std::endl;
        std::cout << "���õ�����·��: ";
        for (const auto& path : fontPaths) {
            std::cout << path << " ";
        }
        std::cout << std::endl;
        return nullptr;
    }

    std::cout << "ʹ������·��: " << fontPath << std::endl;

    // ��������ض���С������
    TTF_Font* font = TTF_OpenFont(fontPath.c_str(), size);
    if (!font)
    {
        std::cerr << "��������ʧ��: " << fontPath << " size: " << size << " - " << TTF_GetError() << std::endl;
        return nullptr;
    }

    sizeMap[size] = font;
    std::cout << "�ɹ���������: '" << key << "' size: " << size << std::endl;
    return font;
}

bool ResourceManager::LoadAllFonts()
{
    bool success = true;
    std::cout << "��ʼע������..." << std::endl;

    for (const auto& path : fontPaths)
    {
        if (!LoadFont(path))
        {
            std::cerr << "ע������ʧ��: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "����ע�����" << std::endl;
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
        std::cout << "ж������: " << key << std::endl;
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

// ж���ض������С�ķ���
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
            std::cout << "ж������: " << key << " size: " << size << std::endl;

            // ����������û��������С�ˣ���ȫ�Ƴ�
            if (sizeMap.empty())
            {
                fonts.erase(fontIt);
            }
        }
    }
}

// ����δʹ�������С
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
    std::cout << "������ " << removedCount << " ����������Ŀ" << std::endl;
}

bool ResourceManager::LoadAllSounds()
{
    bool success = true;
    std::cout << "��ʼ������Ч..." << std::endl;

    for (const auto& path : soundPaths)
    {
        std::string key = "sound_" + path.substr(path.find_last_of("/\\") + 1);
        key = key.substr(0, key.find_last_of('.'));
        if (!LoadSound(path, key))
        {
            std::cerr << "������Чʧ��: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "��Ч�������" << std::endl;
    return success;
}

bool ResourceManager::LoadAllMusic()
{
    bool success = true;
    std::cout << "��ʼ��������..." << std::endl;

    for (const auto& path : musicPaths)
    {
        std::string key = "music_" + path.substr(path.find_last_of("/\\") + 1);
        key = key.substr(0, key.find_last_of('.'));
        if (!LoadMusic(path, key))
        {
            std::cerr << "��������ʧ��: " << path << std::endl;
            success = false;
        }
    }

    std::cout << "���ּ������" << std::endl;
    return success;
}

bool ResourceManager::LoadAllAnimations()
{
    if (!renderer) {
        std::cerr << "����: ResourceManager δ��ʼ�����޷����ض�����" << std::endl;
        return false;
    }

    bool success = true;
    int loadedCount = 0;
    std::cout << "��ʼ�������ж�����Դ..." << std::endl;

    for (const auto& animPair : animationPaths) {
        AnimationType animType = animPair.first;
        const std::string& path = animPair.second;

        if (LoadAnimation(animType)) {
            loadedCount++;
            std::cout << "�ɹ����ض���: " << path << " (����: " << static_cast<int>(animType) << ")" << std::endl;
        }
        else {
            success = false;
            std::cerr << "���ض���ʧ��: " << path << " (����: " << static_cast<int>(animType) << ")" << std::endl;
        }
    }

    std::cout << "������Դ������ɣ��ɹ�: " << loadedCount << "/" << animationPaths.size() << std::endl;
    return success;
}

void ResourceManager::UnloadAnimation(AnimationType animType)
{
    auto it = animations.find(animType);
    if (it != animations.end()) {
        it->second->Clear();
        animations.erase(it);
        std::cout << "ж�ض���: " << static_cast<int>(animType) << std::endl;
    }
}

// ��ȡ��Դ·���б�
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

    // �������δ���أ�������������
    std::cout << "�������� " << static_cast<int>(animType) << " δ���أ�������������..." << std::endl;
    if (LoadAnimation(animType)) {
        return animations[animType];
    }

    std::cerr << "����: �޷���ȡ�������� " << static_cast<int>(animType) << std::endl;
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
        std::cerr << "����: ResourceManager δ��ʼ����" << std::endl;
        return nullptr;
    }

    std::string actualKey = key.empty() ? path : key;

    // ����Ƿ��Ѽ���
    if (textures.find(actualKey) != textures.end())
    {
        return textures[actualKey];
    }

    // ��������
    SDL_Texture* texture = IMG_LoadTexture(renderer, path.c_str());
    if (!texture)
    {
        std::cerr << "��������ʧ��: " << path << " - " << IMG_GetError() << std::endl;
        return nullptr;
    }

    textures[actualKey] = texture;
    std::cout << "�ɹ���������: " << path << " (key: " << actualKey << ")" << std::endl;
    return texture;
}

SDL_Texture* ResourceManager::GetTexture(const std::string& key)
{
    auto it = textures.find(key);
    if (it != textures.end())
    {
        return it->second;
    }
    std::cerr << "����: ����δ�ҵ�: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadTexture(const std::string& key)
{
    auto it = textures.find(key);
    if (it != textures.end())
    {
        SDL_DestroyTexture(it->second);
        textures.erase(it);
        std::cout << "ж������: " << key << std::endl;
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
        std::cerr << "������Чʧ��: " << path << " - " << Mix_GetError() << std::endl;
        return nullptr;
    }

    sounds[actualKey] = sound;
    std::cout << "�ɹ�������Ч: " << path << std::endl;
    return sound;
}

Mix_Chunk* ResourceManager::GetSound(const std::string& key)
{
    auto it = sounds.find(key);
    if (it != sounds.end())
    {
        return it->second;
    }
    std::cerr << "����: ��Чδ�ҵ�: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadSound(const std::string& key)
{
    auto it = sounds.find(key);
    if (it != sounds.end())
    {
        Mix_FreeChunk(it->second);
        sounds.erase(it);
        std::cout << "ж����Ч: " << key << std::endl;
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
        std::cerr << "��������ʧ��: " << path << " - " << Mix_GetError() << std::endl;
        return nullptr;
    }

    this->music[actualKey] = music;
    std::cout << "�ɹ���������: " << path << std::endl;
    return music;
}

Mix_Music* ResourceManager::GetMusic(const std::string& key)
{
    auto it = music.find(key);
    if (it != music.end())
    {
        return it->second;
    }
    std::cerr << "����: ����δ�ҵ�: " << key << std::endl;
    return nullptr;
}

void ResourceManager::UnloadMusic(const std::string& key)
{
    auto it = music.find(key);
    if (it != music.end())
    {
        Mix_FreeMusic(it->second);
        music.erase(it);
        std::cout << "ж������: " << key << std::endl;
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
        std::cerr << "����: ResourceManager δ��ʼ����" << std::endl;
        return false;
    }

    if (animations.find(animType) != animations.end()) {
        return true;
    }

    auto pathIt = animationPaths.find(animType);
    if (pathIt == animationPaths.end()) {
        std::cerr << "����: δ�ҵ��������� " << static_cast<int>(animType) << " ��·������" << std::endl;
        return false;
    }

    const std::string& path = pathIt->second;

    auto definition = std::make_shared<ReanimatorDefinition>();
    if (!definition->LoadFromFile(path)) {
        std::cerr << "���ض����ļ�ʧ��: " << path << std::endl;
        return false;
    }

    if (!definition->LoadImages(renderer)) {
        std::cerr << "����: ����ͼƬ����ʧ�ܻ򲿷�ʧ��: " << path << std::endl;
    }

    animations[animType] = definition;
    return true;
}

void ResourceManager::UnloadAll()
{
    // ж����������
    for (auto& pair : textures)
    {
        SDL_DestroyTexture(pair.second);
    }
    textures.clear();

    // ж����������
    for (auto& fontPair : fonts)
    {
        for (auto& sizePair : fontPair.second)
        {
            TTF_CloseFont(sizePair.second);
        }
    }
    fonts.clear();

    // ж��������Ч
    for (auto& pair : sounds)
    {
        Mix_FreeChunk(pair.second);
    }
    sounds.clear();

    // ж����������
    for (auto& pair : music)
    {
        Mix_FreeMusic(pair.second);
    }
    music.clear();

    // ж�����ж���
    for (auto& pair : animations) 
    {
        pair.second->Clear();
    }
    animations.clear();

    std::cout << "��ж��������Դ" << std::endl;
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