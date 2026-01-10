#pragma once
#ifndef _RESOURCE_KEYS_H
#define _RESOURCE_KEYS_H

#include <string>
#include <algorithm>

namespace ResourceKeys
{
    namespace Textures
    {
        const std::string IMAGE_KID = "IMAGE_KID";
        const std::string IMAGE_OPTIONS_CHECKBOX0 = "IMAGE_OPTIONS_CHECKBOX0";
        const std::string IMAGE_OPTIONS_CHECKBOX1 = "IMAGE_OPTIONS_CHECKBOX1";
        const std::string IMAGE_SEEDCHOOSER_BUTTON2 = "IMAGE_SEEDCHOOSER_BUTTON2";
        const std::string IMAGE_SEEDCHOOSER_BUTTON2_GLOW = "IMAGE_SEEDCHOOSER_BUTTON2_GLOW";
        const std::string IMAGE_OPTIONS_SLIDERKNOB2 = "IMAGE_OPTIONS_SLIDERKNOB2";
        const std::string IMAGE_OPTIONS_SLIDERSLOT = "IMAGE_OPTIONS_SLIDERSLOT";
        const std::string IMAGE_SELECTORSCREEN_ADVENTURE_BUTTON = "IMAGE_SELECTORSCREEN_ADVENTURE_BUTTON";
        const std::string IMAGE_SELECTORSCREEN_ADVENTURE_HIGHLIGHT = "IMAGE_SELECTORSCREEN_ADVENTURE_HIGHLIGHT";
        const std::string IMAGE_SELECTORSCREEN_ALMANAC = "IMAGE_SELECTORSCREEN_ALMANAC";
        const std::string IMAGE_SELECTORSCREEN_ALMANACHIGHLIGHT = "IMAGE_SELECTORSCREEN_ALMANACHIGHLIGHT";
        const std::string IMAGE_SELECTORSCREEN_BG = "IMAGE_SELECTORSCREEN_BG";
        const std::string IMAGE_SELECTORSCREEN_BG_CENTER = "IMAGE_SELECTORSCREEN_BG_CENTER";
        const std::string IMAGE_SELECTORSCREEN_BG_LEFT = "IMAGE_SELECTORSCREEN_BG_LEFT";
        const std::string IMAGE_SELECTORSCREEN_BG_RIGHT = "IMAGE_SELECTORSCREEN_BG_RIGHT";
        const std::string IMAGE_SELECTORSCREEN_CHALLENGES_BUTTON = "IMAGE_SELECTORSCREEN_CHALLENGES_BUTTON";
        const std::string IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT = "IMAGE_SELECTORSCREEN_CHALLENGES_HIGHLIGHT";
        const std::string IMAGE_SELECTORSCREEN_FLOWER1 = "IMAGE_SELECTORSCREEN_FLOWER1";
        const std::string IMAGE_SELECTORSCREEN_FLOWER2 = "IMAGE_SELECTORSCREEN_FLOWER2";
        const std::string IMAGE_SELECTORSCREEN_FLOWER3 = "IMAGE_SELECTORSCREEN_FLOWER3";
        const std::string IMAGE_SELECTORSCREEN_GAME = "IMAGE_SELECTORSCREEN_GAME";
        const std::string IMAGE_SELECTORSCREEN_GAME_SHADOW = "IMAGE_SELECTORSCREEN_GAME_SHADOW";
        const std::string IMAGE_SELECTORSCREEN_HELP1 = "IMAGE_SELECTORSCREEN_HELP1";
        const std::string IMAGE_SELECTORSCREEN_HELP2 = "IMAGE_SELECTORSCREEN_HELP2";
        const std::string IMAGE_SELECTORSCREEN_OPTIONS1 = "IMAGE_SELECTORSCREEN_OPTIONS1";
        const std::string IMAGE_SELECTORSCREEN_OPTIONS2 = "IMAGE_SELECTORSCREEN_OPTIONS2";
        const std::string IMAGE_SELECTORSCREEN_QUIT1 = "IMAGE_SELECTORSCREEN_QUIT1";
        const std::string IMAGE_SELECTORSCREEN_QUIT2 = "IMAGE_SELECTORSCREEN_QUIT2";
        const std::string IMAGE_SELECTORSCREEN_STARTADVENTURE_BUTTON1 = "IMAGE_SELECTORSCREEN_STARTADVENTURE_BUTTON1";
        const std::string IMAGE_SELECTORSCREEN_STARTADVENTURE_HIGHLIGHT = "IMAGE_SELECTORSCREEN_STARTADVENTURE_HIGHLIGHT";
        const std::string IMAGE_SELECTORSCREEN_STORE = "IMAGE_SELECTORSCREEN_STORE";
        const std::string IMAGE_SELECTORSCREEN_STOREHIGHLIGHT = "IMAGE_SELECTORSCREEN_STOREHIGHLIGHT";
        const std::string IMAGE_SELECTORSCREEN_SURIVAL = "IMAGE_SELECTORSCREEN_SURIVAL";
        const std::string IMAGE_SELECTORSCREEN_SURIVAL_SHADOW = "IMAGE_SELECTORSCREEN_SURIVAL_SHADOW";
        const std::string IMAGE_CARD_BK = "IMAGE_CARD_BK";
        const std::string IMAGE_SEEDBANK_LONG = "IMAGE_SEEDBANK_LONG";
        const std::string IMAGE_SEEDCHOOSER_BACKGROUND = "IMAGE_SEEDCHOOSER_BACKGROUND";
        const std::string IMAGE_SEEDCHOOSER_BUTTON = "IMAGE_SEEDCHOOSER_BUTTON";
        const std::string IMAGE_SEEDCHOOSER_BUTTON_DISABLED = "IMAGE_SEEDCHOOSER_BUTTON_DISABLED";
        const std::string IMAGE_SEEDPACKETNORMAL = "IMAGE_SEEDPACKETNORMAL";
        const std::string IMAGE_BACKGROUND_DAY = "IMAGE_BACKGROUND_DAY";
        const std::string IMAGE_BACKGROUND_NIGHT = "IMAGE_BACKGROUND_NIGHT";
        const std::string IMAGE_SHOVELBANK = "IMAGE_SHOVELBANK";
        const std::string IMAGE_OPTIONS_MENUBACK = "IMAGE_OPTIONS_MENUBACK";
        const std::string IMAGE_BUTTONBIG = "IMAGE_BUTTONBIG";
        const std::string IMAGE_BUTTONSMALL = "IMAGE_BUTTONSMALL";
        const std::string IMAGE_MESSAGEBOX = "IMAGE_MESSAGEBOX";
        const std::string IMAGE_OPTIONS_BACKTOGAMEBUTTON0 = "IMAGE_OPTIONS_BACKTOGAMEBUTTON0";
        const std::string IMAGE_OPTIONS_BACKTOGAMEBUTTON_LIGHT = "IMAGE_OPTIONS_BACKTOGAMEBUTTON_LIGHT";
        const std::string IMAGE_SHOVEL = "IMAGE_SHOVEL";

        // 植物图片（IMAGE_ + 大写文件名）
        const std::string IMAGE_PEASHOOTER = "IMAGE_PEASHOOTER";
        const std::string IMAGE_SUNFLOWER = "IMAGE_SUNFLOWER";
        const std::string IMAGE_CHERRYBOMB = "IMAGE_CHERRYBOMB";
        const std::string IMAGE_WALLNUT = "IMAGE_WALLNUT";
        const std::string IMAGE_POTATOMINE = "IMAGE_POTATOMINE";
        const std::string IMAGE_SNOWPEASHOOTER = "IMAGE_SNOWPEASHOOTER";
        const std::string IMAGE_CHOMPER = "IMAGE_CHOMPER";
        const std::string IMAGE_REPEATER = "IMAGE_REPEATER";
        const std::string IMAGE_PUFFSHROOM = "IMAGE_PUFFSHROOM";
        const std::string IMAGE_SUNSHROOM = "IMAGE_SUNSHROOM";
        const std::string IMAGE_FUMESHROOM = "IMAGE_FUMESHROOM";
        const std::string IMAGE_GRAVEBUSTER = "IMAGE_GRAVEBUSTER";
        const std::string IMAGE_HYPNOSHROOM = "IMAGE_HYPNOSHROOM";
        const std::string IMAGE_SCAREDYSHROOM = "IMAGE_SCAREDYSHROOM";
        const std::string IMAGE_ICESHROOM = "IMAGE_ICESHROOM";
        const std::string IMAGE_DOOMSHROOM = "IMAGE_DOOMSHROOM";
        const std::string IMAGE_SQUASH = "IMAGE_SQUASH";
        const std::string IMAGE_THREEPEATER = "IMAGE_THREEPEATER";
        const std::string IMAGE_TANGLEKELP = "IMAGE_TANGLEKELP";
        const std::string IMAGE_JALAPENO = "IMAGE_JALAPENO";
        const std::string IMAGE_TORCHWOOD = "IMAGE_TORCHWOOD";
    }

    namespace Particles
    {
        const std::string PARTICLE_ZOMBIEHEAD = "PARTICLE_ZOMBIEHEAD";
	}

    namespace Fonts
    {
        const std::string FONT_FZCQ = "./font/fzcq.ttf";
        const std::string FONT_FZJZ = "./font/fzjz.ttf";
        const std::string FONT_CONTINUUMBOLD = "./font/ContinuumBold.ttf";
    }

    namespace Sounds
    {
        const std::string SOUND_BLEEP = "SOUND_BLEEP";
        const std::string SOUND_BUTTONCLICK = "SOUND_BUTTONCLICK";
        const std::string SOUND_CERAMIC = "SOUND_CERAMIC";
        const std::string SOUND_CHOOSEPLANT1 = "SOUND_CHOOSEPLANT1";
        const std::string SOUND_CHOOSEPLANT2 = "SOUND_CHOOSEPLANT2";
        const std::string SOUND_GRAVEBUTTON = "SOUND_GRAVEBUTTON";
        const std::string SOUND_MAINMENU = "SOUND_MAINMENU";
        const std::string SOUND_PAUSE = "SOUND_PAUSE";
        const std::string SOUND_SHOVEL = "SOUND_SHOVEL";
        const std::string SOUND_COLLECTSUN = "SOUND_COLLECTSUN";
        const std::string SOUND_CLICKFAILED = "SOUND_CLICKFAILED";
        const std::string SOUND_AFTERHUGEWAVE = "SOUND_AFTERHUGEWAVE";
        const std::string SOUND_FINALWAVE = "SOUND_FINALWAVE";
        const std::string SOUND_FIRSTWAVE = "SOUND_FIRSTWAVE";
        const std::string SOUND_LAWNMOWER = "SOUND_LAWNMOWER";
        const std::string SOUND_LOSTGAME = "SOUND_LOSTGAME";
        const std::string SOUND_POOL_CLEANER = "SOUND_POOL_CLEANER";
        const std::string SOUND_READYSETPLANT = "SOUND_READYSETPLANT";
        const std::string SOUND_WINMUSIC = "SOUND_WINMUSIC";
        const std::string SOUND_CLICKSEED = "SOUND_CLICKSEED";
        const std::string SOUND_DELETEPLANT = "SOUND_DELETEPLANT";
        const std::string SOUND_PLANT = "SOUND_PLANT";
        const std::string SOUND_PLANT_ONWATER = "SOUND_PLANT_ONWATER";
    }

    namespace Music
    {
        const std::string MUSIC_MAINMENU = "MUSIC_MAINMENU";
    }

    namespace Reanimations
    {
        const std::string REANIM_SUN = "Sun";
        const std::string REANIM_SUNFLOWER = "SunFlower";
		const std::string REANIM_PEASHOOTER = "PeaShooter";
    }

    class ResourceKeyGenerator
    {
    public:
        // 根据路径和前缀生成键名（与ResourceManager中的逻辑完全一致）
        static std::string GenerateKeyFromPath(const std::string& path, const std::string& prefix)
        {
            // 提取文件名
            size_t lastSlash = path.find_last_of("/\\");
            std::string filename = (lastSlash != std::string::npos) ?
                path.substr(lastSlash + 1) : path;

            // 移除扩展名
            size_t dotPos = filename.find_last_of('.');
            std::string nameWithoutExt = (dotPos != std::string::npos) ?
                filename.substr(0, dotPos) : filename;

            // 转换为大写
            std::string upperName = "";
            for (char c : nameWithoutExt) {
                upperName += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            }

            // 将非字母数字字符替换为下划线
            std::string result = "";
            for (char c : upperName) {
                if (std::isalnum(static_cast<unsigned char>(c))) {
                    result += c;
                }
                else {
                    result += '_';
                }
            }

            return prefix + result;
        }

        // 生成纹理键名
        static std::string GenerateTextureKey(const std::string& path)
        {
            return GenerateKeyFromPath(path, "IMAGE_");
        }

        // 生成粒子纹理键名
        static std::string GenerateParticleKey(const std::string& path)
        {
            return GenerateKeyFromPath(path, "PARTICLE_");
        }

        // 生成音效键名
        static std::string GenerateSoundKey(const std::string& path)
        {
            return GenerateKeyFromPath(path, "SOUND_");
        }

        // 生成音乐键名
        static std::string GenerateMusicKey(const std::string& path)
        {
            return GenerateKeyFromPath(path, "MUSIC_");
        }

        // 验证键名格式是否正确
        static bool IsValidKeyFormat(const std::string& key, const std::string& expectedPrefix = "")
        {
            if (!expectedPrefix.empty() && key.find(expectedPrefix) != 0) {
                return false;
            }

            // 检查键名是否只包含大写字母、数字和下划线
            for (char c : key) {
                if (!(std::isupper(c) || std::isdigit(c) || c == '_')) {
                    return false;
                }
            }

            return true;
        }

        // 获取键名前缀
        static std::string GetKeyPrefix(const std::string& key)
        {
            size_t underscorePos = key.find('_');
            if (underscorePos != std::string::npos) {
                return key.substr(0, underscorePos + 1);
            }
            return "";
        }

        // 获取键名主体（去掉前缀）
        static std::string GetKeyBody(const std::string& key)
        {
            size_t underscorePos = key.find('_');
            if (underscorePos != std::string::npos) {
                return key.substr(underscorePos + 1);
            }
            return key;
        }

        // 比较两个键名是否指向同一资源（不区分大小写）
        static bool CompareKeys(const std::string& key1, const std::string& key2)
        {
            std::string upperKey1 = "";
            std::string upperKey2 = "";

            for (char c : key1) upperKey1 += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            for (char c : key2) upperKey2 += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

            return upperKey1 == upperKey2;
        }
    };
}

#endif