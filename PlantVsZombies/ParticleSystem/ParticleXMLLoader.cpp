#include "ParticleXMLLoader.h"
#include "../FileManager.h"
#include "../Logger.h"
#include <sstream>
#include <algorithm>
#include <cctype>

bool ParticleXMLLoader::LoadFromDirectory(const std::string& directory) {
	FileManager fileManager;
	std::vector<std::string> xmlFiles = fileManager.GetFilesInDirectory(directory, ".xml");

	if (xmlFiles.empty()) {
		LOG_WARN("Particle") << "粒子配置目录为空: " << directory;
		return false;
	}

	bool success = true;
	for (const std::string& file : xmlFiles) {
		if (!LoadFromFile(file)) {
			LOG_WARN("Particle") << "加载粒子配置文件失败: " << file;
			success = false;
		}
	}
	LOG_DEBUG("Particle") << "粒子XML配置加载完成: " << effectConfigs.size() << " 个特效";
	return success;
}

bool ParticleXMLLoader::LoadFromFile(const std::string& filePath) {
	pugi::xml_document doc;
	// 统一走 SDL_RWops(FileManager)，使粒子配置在 Android 也能从 APK assets 读取。
	if (!FileManager::LoadXMLFile(filePath, doc)) {
		LOG_ERROR("Particle") << "加载粒子XML文件失败: " << filePath;
		return false;
	}

	// 解析所有发射器节点
	std::vector<EmitterConfig> emitters;
	for (pugi::xml_node emitterNode : doc.children("Emitter")) {
		EmitterConfig emitter = ParseEmitter(emitterNode);
		emitters.push_back(emitter);
	}

	if (emitters.empty()) {
		LOG_WARN("Particle") << "XML文件中没有找到发射器: " << filePath;
		return false;
	}

	// 使用第一个发射器的名称作为特效名称
	std::string effectName = emitters[0].name;
	if (effectName.empty()) {
		LOG_WARN("Particle") << "发射器没有名称: " << filePath;
		return false;
	}

	ParticleEffectConfig effectConfig(effectName);
	effectConfig.emitters = emitters;

	effectConfigs[effectName] = effectConfig;

	LOG_DEBUG("Particle") << "加载粒子特效: " << effectName << " (" << emitters.size() << " 个发射器)";

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

	// 先按关键帧切分：'[a b]' 这类随机范围内部含空格，必须作为整体保留，
	// 因此不能直接按空白切——只有"括号外"的空白才是关键帧分隔符。
	// 这样 ".3,0 [.4 1.0],10 [.4 1.0],70 0,100" 才能被切成 4 个关键帧而不是被
	// 误当成单一随机范围（旧实现见 "text.find('[')" 短路，会把整条轨迹错读成常量）。
	std::vector<std::string> tokens;
	{
		std::string cur;
		bool inBracket = false;
		for (char c : text) {
			if (c == '[') { inBracket = true; cur += c; }
			else if (c == ']') { inBracket = false; cur += c; }
			else if (!inBracket && std::isspace(static_cast<unsigned char>(c))) {
				if (!cur.empty()) { tokens.push_back(cur); cur.clear(); }
			}
			else { cur += c; }
		}
		if (!cur.empty()) tokens.push_back(cur);
	}

	// 解析单个关键帧 token：形如 VALUE[,TIME]，VALUE 可为数字或 "[a b]" 随机范围。
	struct Raw { float val; float time; bool hasTime; bool isRange; float rangeMin; float rangeMax; };
	std::vector<Raw> raws;
	for (const std::string& tok : tokens) {
		// 跳过缓动曲线关键字（如 "0 EaseOut 20" 中的 "EaseOut"）：
		// 本系统的 InterpolationTrack 只做线性插值、未建模曲线，故降级为线性。
		// 以"首字符为字母"判定，可兼容 EaseIn/EaseInOut/Bounce 等未知曲线名。
		if (std::isalpha(static_cast<unsigned char>(tok[0]))) {
			continue;
		}
		// 括号内不含逗号，故 token 中的逗号必为"值/时间"分隔符
		size_t commaPos = tok.find(',');
		std::string valPart = (commaPos == std::string::npos) ? tok : tok.substr(0, commaPos);
		bool hasTime = (commaPos != std::string::npos);
		try {
			float time = 0.0f;
			if (hasTime) {
				time = std::stof(tok.substr(commaPos + 1)) / 100.0f;
			}
			if (!valPart.empty() && valPart.front() == '[') {
				// "[a b]" 关键帧：每个范围本应逐粒子独立采样，但当前 points 为全发射器
				// 共享、无逐粒子存储，故取范围中点作为确定性值——既恢复正确的缩放/淡出
				// 曲线，又零额外分配。逐粒子尺寸抖动留作后续可选增强。
				std::string inner = valPart.substr(1);
				if (!inner.empty() && inner.back() == ']') inner.pop_back();
				std::istringstream riss(inner);
				float a = 0.0f, b = 0.0f;
				riss >> a;
				if (riss >> b) {
					raws.push_back({ (a + b) * 0.5f, time, hasTime, true, a, b });
				}
				else {
					raws.push_back({ a, time, hasTime, false, a, a });
				}
			}
			else {
				float v = std::stof(valPart);
				raws.push_back({ v, time, hasTime, false, v, v });
			}
		}
		catch (const std::exception& e) {
			LOG_WARN("Particle") << "无法解析关键帧 token \"" << tok << "\" (来源: " << text << "): " << e.what();
		}
	}

	if (raws.empty()) {
		return track;
	}

	// 纯单一随机范围 "[a b]"（仅一个关键帧、是范围、无显式时间）：
	// 保留原有"每粒子生成时随机一次、整生命周期保持"语义（由 baseScale 消费）。
	if (raws.size() == 1) {
		if (raws[0].isRange) {
			track.isConstant = false;
			track.isRandomRange = true;
			track.randomMin = raws[0].rangeMin;
			track.randomMax = raws[0].rangeMax;
			return track;
		}
		track.isConstant = true;
		track.constantValue = raws[0].val;
		return track;
	}

	// 给无显式时间的关键帧填时间：
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
		// 同时保留中点(r.val，供确定性 GetValue)与区间端点(rangeMin/Max，供逐粒子
		// GetValueRandomized)。非区间关键帧 rangeMin==rangeMax==val，两条路径一致。
		track.points.push_back(InterpolationPoint(r.val, r.time, r.rangeMin, r.rangeMax));
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
		LOG_WARN("Particle") << "无法解析数值: " << text;
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