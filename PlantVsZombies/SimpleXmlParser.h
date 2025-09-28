#pragma once
#ifndef _SIMPLE_XML_PARSER_H
#define _SIMPLE_XML_PARSER_H

#include <string>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

class SimpleXmlNode {
public:
    std::string name;
    std::string text;
    std::map<std::string, std::string> attributes;
    std::vector<SimpleXmlNode> children;

    bool GetAttribute(const std::string& attrName, std::string& value) const;
    SimpleXmlNode* GetFirstChild(const std::string& childName);
};

class SimpleXmlParser {
public:
    static bool ParseFile(const std::string& filename, SimpleXmlNode& root);

private:
    static bool ParseReanimContent(const std::string& content, SimpleXmlNode& root);
    static void TrimString(std::string& str);
};

#endif