#include "ParticleXMLLoader.h"
#include "../FileManager.h"
#include <iostream>
#include <sstream>
#include <algorithm>

bool ParticleXMLLoader::LoadFromDirectory(const std::string& directory) {
    FileManager fileManager;
    std::vector<std::string> xmlFiles = fileManager.GetFilesInDirectory(directory, ".xml");

    if (xmlFiles.empty()) {
        std::cerr << "警告: 粒子配置目录为空: " << directory << std::endl;
        return false;
    }

    bool success = true;
    for (const std::string& file : xmlFiles) {
        if (!LoadFromFile(file)) {
            std::cerr << "警告: 加载粒子配置文件失败: " << file << std::endl;
            success = false;
        }
    }
#ifdef _DEBUG
    std::cout << "粒子XML配置加载完成: " << effectConfigs.size() << " 个特效" << std::endl;
#endif
    return success;
}

bool ParticleXMLLoader::LoadFromFile(const std::string& filePath) {
    pugi::xml_document doc;
    pugi::xml_parse_result result = doc.load_file(filePath.c_str());

    if (!result) {
        std::cerr << "加载粒子XML文件失败: " << filePath
            << "，错误: " << result.description() << std::endl;
        return false;
    }

    // 解析所有发射器节点
    std::vector<EmitterConfig> emitters;
    for (pugi::xml_node emitterNode : doc.children("Emitter")) {
        EmitterConfig emitter = ParseEmitter(emitterNode);
        emitters.push_back(emitter);
    }

    if (emitters.empty()) {
        std::cerr << "警告: XML文件中没有找到发射器: " << filePath << std::endl;
        return false;
    }

    // 使用第一个发射器的名称作为特效名称
    std::string effectName = emitters[0].name;
    if (effectName.empty()) {
        std::cerr << "警告: 发射器没有名称: " << filePath << std::endl;
        return false;
    }

    ParticleEffectConfig effectConfig(effectName);
    effectConfig.emitters = emitters;

    effectConfigs[effectName] = effectConfig;

#ifdef _DEBUG
    std::cout << "加载粒子特效: " << effectName << " (" << emitters.size() << " 个发射器)" << std::endl;
#endif

    return true;
}

const ParticleEffectConfig* ParticleXMLLoader::GetEffectConfig(const std::string& name) const {
    auto it = effectConfigs.find(name);
    if (it != effectConfigs.end()) {
        return &it->second;
    }
    return nullptr;
}

std::vector<std::string> ParticleXMLLoader::GetAllEffectNames() const {
    std::vector<std::string> names;
    for (const auto& pair : effectConfigs) {
        names.push_back(pair.first);
    }
    return names;
}

EmitterConfig ParticleXMLLoader::ParseEmitter(const pugi::xml_node& emitterNode) {
    EmitterConfig config;

    // 基础属性
    config.name = emitterNode.child_value("Name");
    config.spawnMinActive = emitterNode.child("SpawnMinActive").text().as_int(1);
    config.spawnMaxLaunched = emitterNode.child("SpawnMaxLaunched").text().as_int(1);
    if (emitterNode.child("SpawnRate")) {
        config.spawnRate = emitterNode.child("SpawnRate").text().as_int(0);
    }
    if (emitterNode.child("ParticleDuration")) {
        config.particleDuration = ParseValueRange(emitterNode.child_value("ParticleDuration"));
    }
    config.systemDuration = emitterNode.child("SystemDuration").text().as_float(-1.0f);

    // 粒子外观
    if (emitterNode.child("ParticleAlpha")) {
        config.particleAlpha = ParseInterpolationTrack(emitterNode.child_value("ParticleAlpha"));
    }
    if (emitterNode.child("ParticleScale")) {
        config.particleScale = ParseInterpolationTrack(emitterNode.child_value("ParticleScale"));
    }
    if (emitterNode.child("ParticleStretch")) {
        config.particleStretch = ParseInterpolationTrack(emitterNode.child_value("ParticleStretch"));
    }
    if (emitterNode.child("ParticleBrightness")) {
        config.particleBrightness = ParseValueRange(emitterNode.child_value("ParticleBrightness"));
    }
    if (emitterNode.child("ParticleRed")) {
        config.particleRed = ParseInterpolationTrack(emitterNode.child_value("ParticleRed"));
    }
    if (emitterNode.child("ParticleGreen")) {
        config.particleGreen = ParseInterpolationTrack(emitterNode.child_value("ParticleGreen"));
    }
    if (emitterNode.child("ParticleBlue")) {
        config.particleBlue = ParseInterpolationTrack(emitterNode.child_value("ParticleBlue"));
    }
    if (emitterNode.child("SystemAlpha")) {
        config.systemAlpha = ParseInterpolationTrack(emitterNode.child_value("SystemAlpha"));
    }

    // 发射器形状
    if (emitterNode.child("EmitterType")) {
        config.emitterType = ParseEmitterType(emitterNode.child_value("EmitterType"));
    }
    if (emitterNode.child("EmitterBoxX")) {
        config.emitterBoxX = ParseValueRange(emitterNode.child_value("EmitterBoxX"));
    }
    if (emitterNode.child("EmitterBoxY")) {
        config.emitterBoxY = ParseValueRange(emitterNode.child_value("EmitterBoxY"));
    }
    if (emitterNode.child("EmitterRadius")) {
        config.emitterRadius = ParseValueRange(emitterNode.child_value("EmitterRadius"));

        // 自动检测：如果未指定 EmitterType 但存在 EmitterRadius，使用 CIRCLE
        if (!emitterNode.child("EmitterType")) {
            config.emitterType = EmitterType::CIRCLE;
        }
    }
    config.emitterOffsetX = emitterNode.child("EmitterOffsetX").text().as_float(0.0f);
    config.emitterOffsetY = emitterNode.child("EmitterOffsetY").text().as_float(0.0f);

    // 运动属性
    if (emitterNode.child("LaunchSpeed")) {
        config.launchSpeed = ParseValueRange(emitterNode.child_value("LaunchSpeed"));
    }
    config.randomLaunchSpin = emitterNode.child("RandomLaunchSpin").text().as_bool(false);
    if (emitterNode.child("ParticleSpinSpeed")) {
        config.particleSpinSpeed = ParseValueRange(emitterNode.child_value("ParticleSpinSpeed"));
    }
    config.particleGravity = emitterNode.child("ParticleGravity").text().as_float(0.0f);

    // 动画
    if (emitterNode.child("Image")) {
        config.imageKeys = ParseImageKeys(emitterNode.child_value("Image"));
    }
    config.imageFrames = emitterNode.child("ImageFrames").text().as_int(1);
    config.animationRate = emitterNode.child("AnimationRate").text().as_float(12.0f);

    // 场效果
    for (pugi::xml_node fieldNode : emitterNode.children("Field")) {
        config.fields.push_back(ParseField(fieldNode));
    }
    for (pugi::xml_node fieldNode : emitterNode.children("SystemField")) {
        config.systemFields.push_back(ParseField(fieldNode));
    }

    // 特殊属性
    config.fullScreen = emitterNode.child("FullScreen").text().as_bool(false);

    return config;
}

InterpolationTrack ParticleXMLLoader::ParseInterpolationTrack(const std::string& text) {
    InterpolationTrack track;

    if (text.empty()) {
        return track;
    }

    std::istringstream iss(text);
    std::vector<float> values;
    float value;

    // 读取所有数值
    while (iss >> value) {
        values.push_back(value);
    }

    if (values.empty()) {
        return track;
    }

    // 如果只有一个值，是常量
    if (values.size() == 1) {
        track.isConstant = true;
        track.constantValue = values[0];
        return track;
    }

    // 解析插值点（格式: value1,time1 value2,time2 ...）
    // 或简化格式: value1 value2（时间均匀分布）
    track.isConstant = false;

    // 检查是否有逗号（完整格式）
    if (text.find(',') != std::string::npos) {
        // 完整格式: "1,90 0"
        std::istringstream iss2(text);
        std::string token;
        while (iss2 >> token) {
            size_t commaPos = token.find(',');
            if (commaPos != std::string::npos) {
                float val = std::stof(token.substr(0, commaPos));
                float time = std::stof(token.substr(commaPos + 1));
                track.points.push_back(InterpolationPoint(val, time / 100.0f));  // 归一化
            }
        }
    }
    else {
        // 简化格式: "1 0"（时间均匀分布）
        for (size_t i = 0; i < values.size(); i++) {
            float time = static_cast<float>(i) / static_cast<float>(values.size() - 1);
            track.points.push_back(InterpolationPoint(values[i], time));
        }
    }

    return track;
}

ValueRange ParticleXMLLoader::ParseValueRange(const std::string& text) {
    if (text.empty()) {
        return ValueRange(0.0f);
    }

    // 检查是否是范围格式 "[min max]"
    if (text.find('[') != std::string::npos) {
        std::string cleaned = text;
        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '['), cleaned.end());
        cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), ']'), cleaned.end());

        std::istringstream iss(cleaned);
        float min, max;
        if (iss >> min >> max) {
            return ValueRange(min, max);
        }
    }

    // 单个值
    float value = std::stof(text);
    return ValueRange(value);
}

ParticleField ParticleXMLLoader::ParseField(const pugi::xml_node& fieldNode) {
    ParticleField field;

    field.type = ParseFieldType(fieldNode.child_value("FieldType"));

    if (fieldNode.child("X")) {
        field.xTrack = ParseInterpolationTrack(fieldNode.child_value("X"));
    }
    if (fieldNode.child("Y")) {
        field.yTrack = ParseInterpolationTrack(fieldNode.child_value("Y"));
    }

    return field;
}

ParticleFieldType ParticleXMLLoader::ParseFieldType(const std::string& typeStr) {
    if (typeStr == "Position") {
        return ParticleFieldType::POSITION;
    }
    else if (typeStr == "Shake") {
        return ParticleFieldType::SHAKE;
    }
    else if (typeStr == "SystemPosition") {
        return ParticleFieldType::SYSTEM_POSITION;
    }
    else if (typeStr == "Friction") {
        return ParticleFieldType::FRICTION;
    }
    return ParticleFieldType::POSITION;
}

EmitterType ParticleXMLLoader::ParseEmitterType(const std::string& typeStr) {
    if (typeStr == "Box") {
        return EmitterType::BOX;
    }
    else if (typeStr == "Circle") {
        return EmitterType::CIRCLE;
    }
    return EmitterType::POINT;
}

std::vector<std::string> ParticleXMLLoader::ParseImageKeys(const std::string& text) {
    std::vector<std::string> keys;

    if (text.empty()) {
        return keys;
    }

    // 检查是否有逗号分隔
    if (text.find(',') != std::string::npos) {
        std::istringstream iss(text);
        std::string key;
        while (std::getline(iss, key, ',')) {
            // 去除前后空格
            key.erase(0, key.find_first_not_of(" \t\r\n"));
            key.erase(key.find_last_not_of(" \t\r\n") + 1);
            if (!key.empty()) {
                keys.push_back(key);
            }
        }
    }
    else {
        keys.push_back(text);
    }

    return keys;
}