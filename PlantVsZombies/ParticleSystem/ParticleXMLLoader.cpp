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
	if (emitterNode.child("SpawnMinActive")) {
		config.spawnMinActive = ParseValueRange(emitterNode.child_value("SpawnMinActive"));
	}
	if (emitterNode.child("SpawnMaxLaunched")) {
		config.spawnMaxLaunched = ParseValueRange(emitterNode.child_value("SpawnMaxLaunched"));
	}
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

	// "[a b]" 随机范围语义：每粒子在生成时随机一次、整生命周期保持
	if (text.find('[') != std::string::npos) {
		std::string cleaned = text;
		cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '['), cleaned.end());
		cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), ']'), cleaned.end());

		std::istringstream iss(cleaned);
		float a = 0.0f, b = 0.0f;
		if (iss >> a >> b) {
			track.isConstant = false;
			track.isRandomRange = true;
			track.randomMin = a;
			track.randomMax = b;
			return track;
		}
		if (!cleaned.empty()) {
			std::istringstream iss2(cleaned);
			if (iss2 >> a) {
				track.isConstant = true;
				track.constantValue = a;
				return track;
			}
		}
		return track;
	}

	// 切分 token，记录是否带显式时间（comma 形式）
	struct Raw { float val; float time; bool hasTime; };
	std::vector<Raw> raws;
	std::istringstream iss(text);
	std::string token;
	while (iss >> token) {
		size_t commaPos = token.find(',');
		try {
			if (commaPos != std::string::npos) {
				float val = std::stof(token.substr(0, commaPos));
				float t = std::stof(token.substr(commaPos + 1)) / 100.0f;
				raws.push_back({ val, t, true });
			}
			else {
				raws.push_back({ std::stof(token), 0.0f, false });
			}
		}
		catch (const std::exception& e) {
			std::cerr << "警告: 无法解析关键帧 token \"" << token
				<< "\" (来源: " << text << "): " << e.what() << std::endl;
		}
	}

	if (raws.empty()) {
		return track;
	}
	if (raws.size() == 1) {
		track.isConstant = true;
		track.constantValue = raws[0].val;
		return track;
	}

	// 给无显式时间的 token 填时间：
	// - 全部无显式 → 均匀分布 0..1（兼容 "1 0"）
	// - 否则 → 首默认 0.0，尾默认 1.0，中间在前后锚点间均匀分布（兼容 "1,80 0" / "0.0,40 .1"）
	bool anyExplicit = false;
	for (const auto& r : raws) {
		if (r.hasTime) { anyExplicit = true; break; }
	}
	if (!anyExplicit) {
		for (size_t i = 0; i < raws.size(); ++i) {
			raws[i].time = static_cast<float>(i) / static_cast<float>(raws.size() - 1);
		}
	}
	else {
		if (!raws.front().hasTime) {
			raws.front().time = 0.0f;
			raws.front().hasTime = true;
		}
		if (!raws.back().hasTime) {
			raws.back().time = 1.0f;
			raws.back().hasTime = true;
		}
		for (size_t i = 1; i + 1 < raws.size(); ++i) {
			if (raws[i].hasTime) continue;
			size_t prev = i - 1;
			size_t next = i + 1;
			while (next < raws.size() && !raws[next].hasTime) ++next;
			float t0 = raws[prev].time;
			float t1 = (next < raws.size()) ? raws[next].time : 1.0f;
			size_t span = next - prev;
			for (size_t j = i; j < next; ++j) {
				raws[j].time = t0 + (t1 - t0) * static_cast<float>(j - prev) / static_cast<float>(span);
				raws[j].hasTime = true;
			}
			i = next - 1;  // 跳过已填的中间段
		}
	}

	track.isConstant = false;
	track.isRandomRange = false;
	for (const auto& r : raws) {
		track.points.push_back(InterpolationPoint(r.val, r.time));
	}
	return track;
}

ValueRange ParticleXMLLoader::ParseValueRange(const std::string& text) {
	if (text.empty()) {
		return ValueRange(0.0f);
	}

	// 一律先剥括号，避免后续 stof/流读取拿到 '['/']'
	std::string cleaned = text;
	cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '['), cleaned.end());
	cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), ']'), cleaned.end());

	std::istringstream iss(cleaned);
	float v1 = 0.0f, v2 = 0.0f;
	if (!(iss >> v1)) {
		std::cerr << "警告: 无法解析数值: " << text << std::endl;
		return ValueRange(0.0f);
	}
	if (iss >> v2) {
		return ValueRange(v1, v2);
	}
	return ValueRange(v1);
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
	else if (typeStr == "Acceleration") {
		return ParticleFieldType::ACCELERATION;
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