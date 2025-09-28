#include "SimpleXmlParser.h"
#include "TodDebug.h"
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

bool SimpleXmlNode::GetAttribute(const std::string& attrName, std::string& value) const {
    auto it = attributes.find(attrName);
    if (it != attributes.end()) {
        value = it->second;
        return true;
    }
    return false;
}

SimpleXmlNode* SimpleXmlNode::GetFirstChild(const std::string& childName) {
    for (auto& child : children) {
        if (child.name == childName) {
            return &child;
        }
    }
    return nullptr;
}

bool SimpleXmlParser::ParseFile(const std::string& filename, SimpleXmlNode& root) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        TOD_TRACE("Failed to open XML file: " + filename);
        return false;
    }

    // ��ȡ�����ļ�����
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();
    file.close();

    return ParseReanimContent(content, root);
}

bool SimpleXmlParser::ParseReanimContent(const std::string& content, SimpleXmlNode& root) {
    root.name = "reanim";

    size_t pos = 0;
    std::vector<SimpleXmlNode*> nodeStack;
    nodeStack.push_back(&root);

    while (pos < content.length()) {
        // �����հ��ַ�
        while (pos < content.length() && std::isspace(content[pos])) {
            pos++;
        }

        if (pos >= content.length()) break;

        // ����Ƿ��ǿ�ʼ��ǩ
        if (content[pos] == '<') {
            size_t tagStart = pos;
            size_t tagEnd = content.find('>', pos);

            if (tagEnd == std::string::npos) {
                TOD_TRACE("Malformed XML: Unclosed tag");
                return false;
            }

            std::string tag = content.substr(pos + 1, tagEnd - pos - 1);
            pos = tagEnd + 1;

            // ����Ƿ��ǽ�����ǩ
            if (tag[0] == '/') {
                std::string endTagName = tag.substr(1);
                if (nodeStack.empty() || nodeStack.back()->name != endTagName) {
                    TOD_TRACE("XML tag mismatch: expected </" + nodeStack.back()->name + ">, found </" + endTagName + ">");
                    return false;
                }
                nodeStack.pop_back();
            }
            // ����Ƿ����Ապϱ�ǩ������reanim�ļ�û���Ապϱ�ǩ��
            else if (tag.back() == '/') {
                // �Ապϱ�ǩ����ʱ������
            }
            // ��ʼ��ǩ
            else {
                SimpleXmlNode* currentNode = nodeStack.back();
                SimpleXmlNode newNode;
                newNode.name = tag;

                currentNode->children.push_back(newNode);
                nodeStack.push_back(&currentNode->children.back());

                //TOD_TRACE("Started tag: " + tag);
            }
        }
        // �����ǩ����
        else {
            SimpleXmlNode* currentNode = nodeStack.back();
            size_t contentStart = pos;
            size_t contentEnd = content.find('<', pos);

            if (contentEnd == std::string::npos) {
                break;
            }

            std::string textContent = content.substr(contentStart, contentEnd - contentStart);
            TrimString(textContent);

            if (!textContent.empty()) {
                currentNode->text = textContent;
                //TOD_TRACE("Tag content: " + currentNode->name + " = " + textContent);
            }

            pos = contentEnd;
        }
    }

    return true;
}

void SimpleXmlParser::TrimString(std::string& str) {
    str.erase(0, str.find_first_not_of(" \t\n\r"));
    str.erase(str.find_last_not_of(" \t\n\r") + 1);
}