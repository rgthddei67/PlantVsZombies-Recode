#pragma once
#ifndef _FILE_MANAGER_H
#define _FILE_MANAGER_H

#include "pugixml.hpp"
#include <fstream>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

class FileManager {
public:
    // 构造函数/析构函数
    FileManager() = default;
    ~FileManager() = default;

    // 检查文件是否存在
    static bool FileExists(const std::string& path);

    // 加载文件为字符串
    static std::string LoadFileAsString(const std::string& path);

    // 加载文件为二进制数据
    static std::vector<char> LoadFileAsBinary(const std::string& path);

    // 保存字符串到文件
    static bool SaveFile(const std::string& path, const std::string& content);

    // 保存二进制数据到文件
    static bool SaveBinaryFile(const std::string& path, const void* data, size_t size);

    // 追加内容到文件
    static bool AppendToFile(const std::string& path, const std::string& content);

    // 加载XML文件
    static pugi::xml_document LoadXMLFile(const std::string& path);
    static bool LoadXMLFile(const std::string& path, pugi::xml_document& doc);

    // 保存XML文件
    static bool SaveXMLFile(const std::string& path, const pugi::xml_document& doc);
    static bool SaveXMLFile(const std::string& path, const pugi::xml_node& node);

    // 加载JSON文件
    static nlohmann::json LoadJsonFile(const std::string& path);
    static bool LoadJsonFile(const std::string& path, nlohmann::json& json);

    // 保存JSON文件
    static bool SaveJsonFile(const std::string& path, const nlohmann::json& json);

    // 获取文件大小
    static size_t GetFileSize(const std::string& path);

    // 创建目录
    static bool CreateDirectory(const std::string& path);

    // 检查是否为目录
    static bool IsDirectory(const std::string& path);

    // 获取文件列表
    static std::vector<std::string> GetFilesInDirectory(const std::string& directory,
        const std::string& extension = "");

    // 路径操作工具函数
    static std::string GetFileName(const std::string& path);
    static std::string GetFileExtension(const std::string& path);
    static std::string GetDirectory(const std::string& path);
    static std::string CombinePath(const std::string& path1, const std::string& path2);

private:
    static void LogError(const std::string& message);
};

inline std::string FileManager::GetFileName(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(pos + 1);
    }
    return path;
}

inline std::string FileManager::GetFileExtension(const std::string& path) {
    size_t dotPos = path.find_last_of('.');
    size_t slashPos = path.find_last_of("/\\");

    if (dotPos != std::string::npos && (slashPos == std::string::npos || dotPos > slashPos)) {
        return path.substr(dotPos);
    }
    return "";
}

inline std::string FileManager::GetDirectory(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) {
        return path.substr(0, pos);
    }
    return "";
}

inline std::string FileManager::CombinePath(const std::string& path1, const std::string& path2) {
    if (path1.empty()) return path2;
    if (path2.empty()) return path1;

    char lastChar = path1.back();
    if (lastChar == '/' || lastChar == '\\') {
        return path1 + path2;
    }
    else {
        return path1 + "/" + path2;
    }
}

#endif