#pragma once
#ifndef _SIMPLE_XML_PARSER_H
#define _SIMPLE_XML_PARSER_H
#include "AllCppInclude.h"
#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

class SimpleXmlNode 
{
public:
    std::string name;
    std::string text;
    std::map<std::string, std::string> attributes;
    std::vector<SimpleXmlNode> children;

    bool GetAttribute(const std::string& attrName, std::string& value) const;
    SimpleXmlNode* GetFirstChild(const std::string& childName);

    std::vector<SimpleXmlNode*> GetChildren(const std::string& childName) {
        std::vector<SimpleXmlNode*> result;
        for (auto& child : children) {
            if (child.name == childName) {
                result.push_back(&child);
            }
        }
        return result;
    }

    std::string GetAttributeValue(const std::string& attrName, const std::string& defaultValue = "") const {
        auto it = attributes.find(attrName);
        if (it != attributes.end()) {
            return it->second;
        }
        return defaultValue;
    }
};

class SimpleXmlParser {
public:
    static bool ParseFile(const std::string& filename, SimpleXmlNode& root);

private:
    static bool ParseReanimContent(const std::string& content, SimpleXmlNode& root);
    static void ParseAttributes(const std::string& tag, SimpleXmlNode& node);
    static void TrimString(std::string& str);
};

#endif