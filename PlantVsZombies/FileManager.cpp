#include "FileManager.h"
#include <iostream>
#include <filesystem>
#include <sstream>

bool FileManager::FileExists(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

std::string FileManager::LoadFileAsString(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        LogError("Failed to open file: " + path);
        return "";
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::vector<char> FileManager::LoadFileAsBinary(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        LogError("Failed to open binary file: " + path);
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (file.read(buffer.data(), size)) {
        return buffer;
    }

    LogError("Failed to read binary file: " + path);
    return {};
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

void FileManager::LogError(const std::string& message) {
    std::cerr << "[FileManager Error] " << message << std::endl;
}