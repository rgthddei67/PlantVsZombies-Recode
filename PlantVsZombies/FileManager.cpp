#include "FileManager.h"
#include <SDL2/SDL.h>
#include "Logger.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

bool FileManager::FileExists(const std::string& path) {
	std::ifstream file(path);
	return file.good();
}

std::string FileManager::LoadFileAsString(const std::string& path) {
	// 复用二进制读取（同样走 SDL_RWops）；空文件与打开失败都返回 ""，与原 ifstream 行为一致。
	std::vector<char> bytes = LoadFileAsBinary(path);
	return std::string(bytes.begin(), bytes.end());
}

std::vector<char> FileManager::LoadFileAsBinary(const std::string& path) {
	// SDL_RWFromFile：相对路径在 Android 自动读 APK assets / 桌面读 CWD；绝对路径走真实文件系统。
	SDL_RWops* rw = SDL_RWFromFile(path.c_str(), "rb");
	if (!rw) {
		LogError("Failed to open file: " + path);
		return {};
	}

	Sint64 size = SDL_RWsize(rw);
	if (size < 0) {
		LogError("Failed to get file size: " + path);
		SDL_RWclose(rw);
		return {};
	}

	std::vector<char> buffer(static_cast<size_t>(size));
	Sint64 readTotal = 0;
	if (size > 0) {
		readTotal = SDL_RWread(rw, buffer.data(), 1, static_cast<size_t>(size));
	}
	SDL_RWclose(rw);

	if (readTotal != size) {
		LogError("Failed to read file: " + path);
		return {};
	}
	return buffer;
}

bool FileManager::SaveFile(const std::string& path, const std::string& content) {
	std::ofstream file(path);
	if (!file.is_open()) {
		LogError("Failed to create file: " + path);
		return false;
	}

	file << content;
	return !file.fail();
}

bool FileManager::SaveBinaryFile(const std::string& path, const void* data, size_t size) {
	std::ofstream file(path, std::ios::binary);
	if (!file.is_open()) {
		LogError("Failed to create binary file: " + path);
		return false;
	}

	file.write(static_cast<const char*>(data), size);
	return !file.fail();
}

bool FileManager::AppendToFile(const std::string& path, const std::string& content) {
	std::ofstream file(path, std::ios::app);
	if (!file.is_open()) {
		LogError("Failed to open file for appending: " + path);
		return false;
	}

	file << content;
	return !file.fail();
}

pugi::xml_document FileManager::LoadXMLFile(const std::string& path) {
	pugi::xml_document doc;
	LoadXMLFile(path, doc);
	return doc;
}

bool FileManager::LoadXMLFile(const std::string& path, pugi::xml_document& doc) {
	std::string content = LoadFileAsString(path);
	if (content.empty()) {
		return false;
	}

	pugi::xml_parse_result result = doc.load_string(content.c_str());
	if (!result) {
		LogError("Failed to parse XML file: " + path + ", error: " + result.description());
		return false;
	}

	return true;
}

bool FileManager::SaveXMLFile(const std::string& path, const pugi::xml_document& doc) {
	std::stringstream ss;
	doc.save(ss);
	return SaveFile(path, ss.str());
}

bool FileManager::SaveXMLFile(const std::string& path, const pugi::xml_node& node) {
	std::stringstream ss;
	node.print(ss);
	return SaveFile(path, ss.str());
}

nlohmann::json FileManager::LoadJsonFile(const std::string& path) {
	nlohmann::json json;
	LoadJsonFile(path, json);
	return json;
}

bool FileManager::LoadJsonFile(const std::string& path, nlohmann::json& json) {
	std::string content = LoadFileAsString(path);
	if (content.empty()) {
		return false;
	}

	try {
		json = nlohmann::json::parse(content);
		return true;
	}
	catch (const nlohmann::json::parse_error& e) {
		LogError("Failed to parse JSON file: " + path + ", error: " + e.what());
		return false;
	}
}

bool FileManager::SaveJsonFile(const std::string& path, const nlohmann::json& json) {
	try {
		std::string content = json.dump(4); // 使用4空格缩进
		return SaveFile(path, content);
	}
	catch (const nlohmann::json::exception& e) {
		LogError("Failed to serialize JSON for file: " + path + ", error: " + e.what());
		return false;
	}
}

size_t FileManager::GetFileSize(const std::string& path) {
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		return 0;
	}
	return file.tellg();
}

bool FileManager::CreateDirectory(const std::string& path) {
	try {
		if (std::filesystem::create_directories(path)) {
			return true;
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		LogError("Failed to create directory: " + path + ", error: " + e.what());
	}
	return false;
}

bool FileManager::IsDirectory(const std::string& path) {
	try {
		return std::filesystem::is_directory(path);
	}
	catch (const std::filesystem::filesystem_error&) {
		return false;
	}
}

std::vector<std::string> FileManager::GetFilesInDirectory(const std::string& directory,
	const std::string& extension) {
	std::vector<std::string> files;

	try {
		for (const auto& entry : std::filesystem::directory_iterator(directory)) {
			if (entry.is_regular_file()) {
				std::string filename = entry.path().string();

				// 如果指定了扩展名，则只添加匹配扩展名的文件
				if (extension.empty() || GetFileExtension(filename) == extension) {
					files.push_back(filename);
				}
			}
		}
	}
	catch (const std::filesystem::filesystem_error& e) {
		LogError("Failed to list files in directory: " + directory + ", error: " + e.what());
	}

	return files;
}

std::vector<std::string> FileManager::ListResourceFiles(const std::string& directory,
	const std::string& extension) {
	// 读构建期烘焙的清单（经 LoadFileAsString = SDL_RWops，APK 可读）；
	// 资源加载在启动期单线程，故首次解析后用函数局部 static 缓存整份清单
	//（C++11 线程安全静态初始化）。每个进程读一次，永远与本次构建产物一致。
	static const std::vector<std::string> manifest = []() {
		std::vector<std::string> lines;
		std::string content = LoadFileAsString("./resources/manifest.txt");
		std::stringstream ss(content);
		std::string line;
		while (std::getline(ss, line)) {
			// 去掉行尾的 \r（CRLF）与首尾空白
			while (!line.empty() && (line.back() == '\r' || line.back() == '\n' ||
				line.back() == ' ' || line.back() == '\t')) {
				line.pop_back();
			}
			if (!line.empty()) {
				lines.push_back(line);
			}
		}
		return lines;
	}();

	// 归一查询目录：去尾部斜杠，使 "./a/b/" 与 "./a/b" 等价。
	std::string dir = directory;
	while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) {
		dir.pop_back();
	}

	// 小写化目标扩展名，便于大小写不敏感比较（空 = 不过滤）。
	std::string extLower = extension;
	std::transform(extLower.begin(), extLower.end(), extLower.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

	std::vector<std::string> files;

	if (!manifest.empty()) {
		for (const std::string& entry : manifest) {
			// 非递归：父目录字符串须与 dir 精确相等（匹配 directory_iterator 语义）。
			if (GetDirectory(entry) != dir) {
				continue;
			}
			if (!extLower.empty()) {
				std::string fileExt = GetFileExtension(entry);
				std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(),
					[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				if (fileExt != extLower) {
					continue;
				}
			}
			// 行内容即与 directory_iterator 逐字一致的全路径（如 ./resources/.../foo.png）。
			files.push_back(entry);
		}
		return files;
	}

	// 清单缺失：桌面回退 std::filesystem（保证桌面永不退化）；Android 必须有清单。
#if defined(__ANDROID__)
	LogError("资源清单缺失且 Android 无文件系统回退: ./resources/manifest.txt");
	return files;
#else
	return GetFilesInDirectory(directory, extension);
#endif
}

bool FileManager::DeleteFile(const std::string& path) {
	try {
		return std::filesystem::remove(path);
	}
	catch (const std::filesystem::filesystem_error& e) {
		LogError("Failed to delete file: " + path + ", error: " + e.what());
		return false;
	}
}

void FileManager::LogError(const std::string& message) {
	LOG_ERROR("FileManager") << message;
}