#include "XflParser.h"
#include "TodDebug.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

XflParser::~XflParser() 
{
    // ��������������Դ
    mDocument.symbols.clear();
    mDocument.bitmaps.clear();

    // ����ʱ��������
    for (auto& layer : mDocument.timeline.layers) {
        layer.frames.clear();
    }
    mDocument.timeline.layers.clear();

    mBasePath.clear();
}

bool XflParser::LoadXflFile(const std::string& filename) {
    mDocument = XflDOMDocument(); // �����ĵ�
    mBasePath.clear();
    // ����ͬ���������
    if (filename.find(".xfl") != std::string::npos) {
        // ���ֱ��ָ����.xfl�ļ�
        mBasePath = fs::path(filename).parent_path().string();
    }
    else {
        // ���ָ�������ļ���
        mBasePath = filename;
        if (mBasePath.back() != '/' && mBasePath.back() != '\\') {
            mBasePath += "/";
        }
    }

    std::string domDocumentPath = mBasePath + "DOMDocument.xml";

    TOD_TRACE("Loading XFL from base path: " + mBasePath);
    TOD_TRACE("DOM Document path: " + domDocumentPath);

    // ����ļ��Ƿ����
    if (!fs::exists(domDocumentPath)) {
        TOD_TRACE("DOMDocument.xml not found at: " + domDocumentPath);
        return false;
    }

    if (!ParseDOMDocument(domDocumentPath)) {
        return false;
    }

    // ����LIBRARY�ļ���
    std::vector<std::string> possibleLibraryPaths = {
        mBasePath + "LIBRARY/",
        mBasePath + "library/",
        mBasePath + "Library/",
        mBasePath  // ֱ���ڵ�ǰĿ¼����
    };

    for (const auto& libraryPath : possibleLibraryPaths) {
        if (fs::exists(libraryPath) && fs::is_directory(libraryPath)) {
            TOD_TRACE("Found library at: " + libraryPath);
            ParseSymbols(libraryPath);
            ParseBitmaps(libraryPath);
            break;
        }
    }

    return true;
}

bool XflParser::ParseDOMDocument(const std::string& filepath) {
    SimpleXmlNode root;
    if (!SimpleXmlParser::ParseFile(filepath, root)) {
        TOD_TRACE("Failed to parse DOMDocument.xml");
        return false;
    }

    // �����ĵ�
    mDocument = XflDOMDocument();

    // ����DOMDocument�ڵ�
    SimpleXmlNode* domDocument = nullptr;
    for (auto& child : root.children) {
        if (child.name == "DOMDocument") {
            domDocument = &child;
            break;
        }
    }

    if (!domDocument) {
        TOD_TRACE("DOMDocument node not found");
        return false;
    }

    // �����ĵ����� - ʹ��GetAttributeValue����
    mDocument.width = std::stoi(domDocument->GetAttributeValue("width", "800"));
    mDocument.height = std::stoi(domDocument->GetAttributeValue("height", "600"));
    mDocument.frameRate = std::stof(domDocument->GetAttributeValue("frameRate", "12.0"));
    mDocument.currentTimeline = std::stoi(domDocument->GetAttributeValue("currentTimeline", "1"));
    mDocument.xflVersion = domDocument->GetAttributeValue("xflVersion", "Unknown");
    mDocument.creatorInfo = domDocument->GetAttributeValue("creatorInfo", "Unknown");
    mDocument.platform = domDocument->GetAttributeValue("platform", "Unknown");
    mDocument.versionInfo = domDocument->GetAttributeValue("versionInfo", "Unknown");

    TOD_TRACE("Document properties - Width: " + std::to_string(mDocument.width) +
        ", Height: " + std::to_string(mDocument.height) +
        ", FPS: " + std::to_string(mDocument.frameRate));

    // ����ʱ����
    for (auto& child : domDocument->children) {
        if (child.name == "timelines") {
            for (auto& timelineNode : child.children) {
                if (timelineNode.name == "DOMTimeline") {
                    if (!ParseTimeline(timelineNode, mDocument.timeline)) {
                        TOD_TRACE("Failed to parse timeline");
                        return false;
                    }
                    break; // ֻ�����һ��ʱ����
                }
            }
        }
    }

    TOD_TRACE("Successfully parsed DOMDocument");
    return true;
}

bool XflParser::ParseTimeline(const SimpleXmlNode& timelineNode, XflDOMTimeline& timeline) {
    // ����ʱ��������
    timeline.name = timelineNode.GetAttributeValue("name", "MainTimeline");

    // ����ͼ��
    for (auto& layerNode : timelineNode.children) {
        if (layerNode.name == "layers") {
            for (auto& layerChild : layerNode.children) {
                if (layerChild.name == "DOMLayer") {
                    XflDOMLayer layer;
                    if (ParseLayer(layerChild, layer)) {
                        timeline.layers.push_back(layer);
                    }
                }
            }
            break; // ֻ�����һ��layers�ڵ�
        }
    }

    TOD_TRACE("Parsed timeline: " + timeline.name + " with " +
        std::to_string(timeline.layers.size()) + " layers");

    return true;
}

bool XflParser::ParseLayer(const SimpleXmlNode& layerNode, XflDOMLayer& layer) {
    // ����ͼ������
    layer.name = layerNode.GetAttributeValue("name", "UnnamedLayer");
    layer.color = layerNode.GetAttributeValue("color", "#4FFF4F");

    std::string lockedStr = layerNode.GetAttributeValue("locked", "false");
    std::string visibleStr = layerNode.GetAttributeValue("visible", "true");

    layer.locked = (lockedStr == "true");
    layer.visible = (visibleStr == "true");

    TOD_TRACE("Parsing layer: " + layer.name + " (visible: " + std::string(layer.visible ? "true" : "false") + ")");

    // ����֡
    for (auto& childNode : layerNode.children) {
        if (childNode.name == "frames") {
            for (auto& frameChild : childNode.children) {
                if (frameChild.name == "DOMFrame") {
                    XflDOMFrame frame;
                    if (ParseFrame(frameChild, frame)) {
                        layer.frames.push_back(frame);
                    }
                }
            }
            break; // ֻ�����һ��frames�ڵ�
        }
    }

    TOD_TRACE("Layer " + layer.name + " has " + std::to_string(layer.frames.size()) + " frames");

    return true;
}

bool XflParser::ParseFrame(const SimpleXmlNode& frameNode, XflDOMFrame& frame) {
    std::string indexStr, durationStr;

    // ��ȫ���� index
    if (frameNode.GetAttribute("index", indexStr)) {
        try {
            frame.index = std::stoi(indexStr);
        }
        catch (const std::exception& e) {
            TOD_TRACE("��Ч��֡����: '" + indexStr + "', ʹ��Ĭ��ֵ 0");
            frame.index = 0;
        }
    }
    else {
        frame.index = 0; // Ĭ��ֵ
    }

    // ��ȫ���� duration
    if (frameNode.GetAttribute("duration", durationStr)) {
        try {
            frame.duration = std::stoi(durationStr);
            if (frame.duration <= 0) {
                TOD_TRACE("��Ч��֡����ʱ��: " + std::to_string(frame.duration) + ", ʹ��Ĭ��ֵ 1");
                frame.duration = 1;
            }
        }
        catch (const std::exception& e) {
            TOD_TRACE("��Ч��֡����ʱ��: '" + durationStr + "', ʹ��Ĭ��ֵ 1");
            frame.duration = 1;
        }
    }
    else {
        frame.duration = 1; // Ĭ��ֵ
    }

    frameNode.GetAttribute("keyMode", frame.keyMode);

    TOD_TRACE("����֡ - ����: " + std::to_string(frame.index) +
        ", ����ʱ��: " + std::to_string(frame.duration));

    // ����Ԫ��
    for (auto& childNode : frameNode.children) {
        if (childNode.name == "elements") {
            for (auto& elementNode : childNode.children) {
                if (elementNode.name == "DOMSymbolInstance" ||
                    elementNode.name == "DOMBitmapInstance") {

                    XflElement element;
                    element.libraryItemName = elementNode.GetAttributeValue("libraryItemName");

                    if (!element.libraryItemName.empty()) {
                        // ��ȫ����������
                        try {
                            // ���Ҿ���ڵ�
                            for (auto& matrixNode : elementNode.children) {
                                if (matrixNode.name == "matrix") {
                                    // ��ȡ�����ı�����
                                    std::string matrixText;
                                    for (auto& matrixChild : matrixNode.children) {
                                        if (matrixChild.name == "Matrix") {
                                            // ��Matrix�ڵ�������й��������ַ���
                                            std::stringstream matrixStream;
                                            matrixStream << "Matrix ";
                                            for (const auto& attr : matrixChild.attributes) {
                                                matrixStream << attr.first << "=\"" << attr.second << "\" ";
                                            }
                                            matrixText = matrixStream.str();
                                            break;
                                        }
                                    }

                                    if (!matrixText.empty()) {
                                        element.matrix = ParseMatrix(matrixText);
                                        element.x = element.matrix.tx;
                                        element.y = element.matrix.ty;

                                        // �������ű���
                                        element.scaleX = sqrtf(element.matrix.a * element.matrix.a + element.matrix.b * element.matrix.b);
                                        element.scaleY = sqrtf(element.matrix.c * element.matrix.c + element.matrix.d * element.matrix.d);

                                        // ������ת�Ƕȣ�����ת�Ƕȣ�
                                        float rotationRad = atan2f(-element.matrix.c, element.matrix.d);
                                        element.rotation = rotationRad * 180.0f / 3.14159265f;

                                        TOD_TRACE("Ԫ�� " + element.libraryItemName +
                                            " - λ��: (" + std::to_string(element.x) + ", " + std::to_string(element.y) +
                                            ") ��ת: " + std::to_string(element.rotation) + "��" +
                                            " ����: (" + std::to_string(element.scaleX) + ", " + std::to_string(element.scaleY) + ")");
                                    }
                                    break;
                                }
                            }

                            // ��ȫ������ɫ
                            for (auto& colorNode : elementNode.children) {
                                if (colorNode.name == "color") {
                                    for (auto& colorChild : colorNode.children) {
                                        if (colorChild.name == "Color") {
                                            std::string alphaStr = colorChild.GetAttributeValue("alphaMultiplier", "1.0");
                                            try {
                                                element.alpha = std::stof(alphaStr);
                                                // ȷ��alphaֵ����Ч��Χ��
                                                if (element.alpha < 0.0f) element.alpha = 0.0f;
                                                if (element.alpha > 1.0f) element.alpha = 1.0f;
                                            }
                                            catch (const std::exception& e) {
                                                TOD_TRACE("��Ч��alphaֵ: '" + alphaStr + "', ʹ��Ĭ��ֵ 1.0");
                                                element.alpha = 1.0f;
                                            }
                                            break;
                                        }
                                    }
                                    break;
                                }
                            }

                            // ��ӵ�֡
                            frame.elements.push_back(element.libraryItemName);
                            frame.elementDetails.push_back(element);

                        }
                        catch (const std::exception& e) {
                            TOD_TRACE("����Ԫ�� '" + element.libraryItemName +
                                "' ʱ����: " + e.what());
                            // ������������Ԫ�ض�������ֹ
                        }
                    }
                }
            }
        }
    }

    TOD_TRACE("֡ " + std::to_string(frame.index) + " ������ɣ����� " +
        std::to_string(frame.elements.size()) + " ��Ԫ��");

    return true;
}

XflMatrix XflParser::ParseMatrix(const std::string& matrixText) {
    XflMatrix matrix;

    TOD_TRACE("���������ı�: " + matrixText);

    // ���XFL�����ʽ: "<Matrix a=\"0.8\" d=\"0.8\" tx=\"7.7\" ty=\"7.9\"/>" 
    if (matrixText.find("Matrix") != std::string::npos) {
        // XML��ʽ�ľ��� - ��<Matrix>�ڵ�����ȡ����
        size_t a_pos = matrixText.find("a=\"");
        size_t b_pos = matrixText.find("b=\"");
        size_t c_pos = matrixText.find("c=\"");
        size_t d_pos = matrixText.find("d=\"");
        size_t tx_pos = matrixText.find("tx=\"");
        size_t ty_pos = matrixText.find("ty=\"");

        auto extractValue = [&](size_t pos, float defaultValue = 0.0f) -> float {
            if (pos == std::string::npos) {
                return defaultValue;
            }

            size_t value_start = pos + 3;
            size_t value_end = matrixText.find("\"", value_start);
            if (value_end == std::string::npos) {
                return defaultValue;
            }

            std::string valueStr = matrixText.substr(value_start, value_end - value_start);

            // ����ַ����Ƿ�Ϊ�ջ�ֻ�����հ��ַ�
            if (valueStr.empty()) {
                TOD_TRACE("���ַ�����ʹ��Ĭ��ֵ: " + std::to_string(defaultValue));
                return defaultValue;
            }

            // ȥ��ǰ��հ�
            valueStr.erase(0, valueStr.find_first_not_of(" \t\n\r"));
            valueStr.erase(valueStr.find_last_not_of(" \t\n\r") + 1);

            if (valueStr.empty()) {
                TOD_TRACE("ֻ�пհ��ַ���ʹ��Ĭ��ֵ: " + std::to_string(defaultValue));
                return defaultValue;
            }

            try {
                return std::stof(valueStr);
            }
            catch (const std::exception& e) {
                TOD_TRACE("��Ч�ľ���ֵ: '" + valueStr + "', ʹ��Ĭ��ֵ " + std::to_string(defaultValue));
                return defaultValue;
            }
            };

        // ����Ĭ��ֵ��a��dĬ��Ϊ1.0������Ĭ��Ϊ0.0
        matrix.a = extractValue(a_pos, 1.0f);
        matrix.b = extractValue(b_pos, 0.0f);
        matrix.c = extractValue(c_pos, 0.0f);
        matrix.d = extractValue(d_pos, 1.0f);
        matrix.tx = extractValue(tx_pos, 0.0f);
        matrix.ty = extractValue(ty_pos, 0.0f);
    }
    else {
        // �����Ƕ��ŷָ�����ֵ��ʽ
        std::vector<float> values;
        std::stringstream ss(matrixText);
        std::string token;

        while (std::getline(ss, token, ',')) {
            // ȥ���հ��ַ�
            token.erase(0, token.find_first_not_of(" \t\n\r"));
            token.erase(token.find_last_not_of(" \t\n\r") + 1);

            if (token.empty()) {
                values.push_back(0.0f);
                continue;
            }

            try {
                values.push_back(std::stof(token));
            }
            catch (const std::exception& e) {
                TOD_TRACE("��Ч�ľ�������: '" + token + "', ʹ�� 0.0");
                values.push_back(0.0f);
            }
        }

        // ����ֵ���������þ���
        if (values.size() >= 6) {
            matrix.a = values[0];
            matrix.b = values[1];
            matrix.c = values[2];
            matrix.d = values[3];
            matrix.tx = values[4];
            matrix.ty = values[5];
        }
        else {
            // ���ֵ���㣬ʹ�õ�λ����
            matrix.a = 1.0f;
            matrix.d = 1.0f;
            TOD_TRACE("����ֵ���㣬ʹ�õ�λ����");
        }
    }

    TOD_TRACE("������ľ���: a=" + std::to_string(matrix.a) +
        " b=" + std::to_string(matrix.b) +
        " c=" + std::to_string(matrix.c) +
        " d=" + std::to_string(matrix.d) +
        " tx=" + std::to_string(matrix.tx) +
        " ty=" + std::to_string(matrix.ty));

    return matrix;
}

XflColor XflParser::ParseColor(const std::string& colorText) {
    XflColor color;

    // �򻯵���ɫ���� - XFL��ɫ��ʽ���ܸ�����
    // ����ֻ���������alpha͸����
    if (colorText.find("alphaMultiplier") != std::string::npos) {
        // ��ȡalphaֵ
        size_t start = colorText.find("alphaMultiplier=\"") + 17;
        size_t end = colorText.find("\"", start);
        if (start != std::string::npos && end != std::string::npos) {
            try {
                color.alphaMultiplier = std::stof(colorText.substr(start, end - start));
            }
            catch (...) {
                color.alphaMultiplier = 1.0f;
            }
        }
    }

    return color;
}

bool XflParser::ParseSymbols(const std::string& libraryFolder) {
    // Ŀǰ��Ҫ��עλͼ
    TOD_TRACE("Parsing symbols from: " + libraryFolder);
    return true;
}

bool XflParser::ParseBitmaps(const std::string& libraryFolder) {
    TOD_TRACE("Scanning for bitmaps in: " + libraryFolder);

    int foundCount = 0;
    for (const auto& entry : fs::directory_iterator(libraryFolder)) {
        if (entry.is_regular_file()) {
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

            if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
                extension == ".bmp" || extension == ".tga") {
                XflBitmapItem bitmap;
                bitmap.name = entry.path().stem().string();
                bitmap.href = entry.path().string();
                mDocument.bitmaps[bitmap.name] = bitmap;
                foundCount++;
                TOD_TRACE("Found bitmap: " + bitmap.name + " at " + bitmap.href);
            }
        }
    }

    // ����λͼ����XML�ļ�
    for (const auto& entry : fs::directory_iterator(libraryFolder)) {
        if (entry.is_regular_file() && entry.path().extension() == ".xml") {
            std::string filename = entry.path().filename().string();
            if (filename.find("Bitmap") != std::string::npos) {
                SimpleXmlNode root;
                if (SimpleXmlParser::ParseFile(entry.path().string(), root)) {
                    for (auto& child : root.children) {
                        if (child.name == "DOMBitmapItem") {
                            XflBitmapItem bitmap;
                            bitmap.name = child.GetAttributeValue("name");
                            bitmap.href = child.GetAttributeValue("href");

                            if (!bitmap.name.empty()) {
                                // ���href�����·����ת��Ϊ����·��
                                if (!bitmap.href.empty() && !fs::path(bitmap.href).is_absolute()) {
                                    bitmap.href = libraryFolder + "/" + bitmap.href;
                                }
                                mDocument.bitmaps[bitmap.name] = bitmap;
                                TOD_TRACE("Parsed bitmap item: " + bitmap.name + " -> " + bitmap.href);
                            }
                        }
                    }
                }
            }
        }
    }

    TOD_TRACE("Total bitmaps found: " + std::to_string(foundCount));
    return true;
}

bool XflParser::GetElementTransform(const std::string& elementName, int frameIndex, XflElement& transform) const {
    // ����ʱ�����в���Ԫ��
    for (const auto& layer : mDocument.timeline.layers) {
        for (const auto& frame : layer.frames) {
            if (frame.index == frameIndex) {
                for (size_t i = 0; i < frame.elements.size(); i++) {
                    if (frame.elements[i] == elementName && i < frame.elementDetails.size()) {
                        transform = frame.elementDetails[i];
                        return true;
                    }
                }
            }
        }
    }
    return false;
}

std::string XflParser::GetBitmapPath(const std::string& bitmapName) const {
    // ���ȼ����ű����Ƿ��ж���
    auto it = mDocument.bitmaps.find(bitmapName);
    if (it != mDocument.bitmaps.end()) {
        // ����ļ��Ƿ����
        if (fs::exists(it->second.href)) {
            return it->second.href;
        }
    }

    // �ڶ�����ܵ�λ������λͼ�ļ�
    std::vector<std::string> extensions = { ".png", ".jpg", ".jpeg", ".bmp", ".tga" };
    std::vector<std::string> searchDirs = {
        mBasePath,
        mBasePath + "../LIBRARY/",
        mBasePath + "../library/",
        "./resources/reanim/LIBRARY/",
        "./resources/reanim/library/",
        "./LIBRARY/",
        "./library/"
    };

    for (const auto& dir : searchDirs) {
        for (const auto& ext : extensions) 
        {
            std::string fullPath = dir + "/LIBRARY" + bitmapName + ext;
            if (fs::exists(fullPath)) {
                TOD_TRACE("Found bitmap: " + fullPath);
                return fullPath;
            }

            // Ҳ����Сд�ļ���
            std::string lowerName = bitmapName;
            std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
            std::string fullPathLower = dir + lowerName + ext;
            if (fs::exists(fullPathLower)) {
                TOD_TRACE("Found bitmap (lowercase): " + fullPathLower);
                return fullPathLower;
            }
        }
    }

    TOD_TRACE("Bitmap not found in any location: " + bitmapName);
    return "";
}