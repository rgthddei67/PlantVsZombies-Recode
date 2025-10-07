#pragma once
#ifndef _XFL_PARSER_H
#define _XFL_PARSER_H
#include <string>
#include <vector>
#include <map>
#include <memory>
#include "SimpleXmlParser.h"

struct XflElement;

// XFL DOM 帧
struct XflDOMFrame {
    int index;
    int duration;
    std::string keyMode;
    std::vector<std::string> elements;  // 保持兼容性
    std::vector<XflElement> elementDetails;  // 存储详细元素信息
    std::map<std::string, std::string> properties;
};

// XFL DOM 图层
struct XflDOMLayer {
    std::string name;
    std::string color;
    bool locked;
    bool visible;
    int layerType;
    std::vector<XflDOMFrame> frames;
};

// XFL DOM 时间轴
struct XflDOMTimeline {
    std::string name;
    std::vector<XflDOMLayer> layers;
};

// XFL 符号项
struct XflSymbolItem {
    std::string name;
    std::string linkageClassName;
    std::string sourceFilePath;
    int scaleGridLeft;
    int scaleGridTop;
    int scaleGridRight;
    int scaleGridBottom;
    XflDOMTimeline timeline;
};

// XFL 位图项
struct XflBitmapItem {
    std::string name;
    std::string linkageClassName;
    std::string sourceFilePath;
    std::string originalCompressionType;
    int quality;
    std::string href; // 实际图片文件路径
};

// XFL DOM 文档
struct XflDOMDocument {
    int width = 100;
    int height = 100;
    float frameRate = 0;
    int currentTimeline = 0;
    std::string xflVersion;
    std::string creatorInfo;
    std::string platform;
    std::string versionInfo;
    XflDOMTimeline timeline;
    std::map<std::string, XflSymbolItem> symbols;
    std::map<std::string, XflBitmapItem> bitmaps;
};

// XFL 元素变换
struct XflMatrix {
    float a, b, c, d, tx, ty;
    XflMatrix() : a(1), b(0), c(0), d(1), tx(0), ty(0) {}
};

struct XflColor {
    float redMultiplier, greenMultiplier, blueMultiplier, alphaMultiplier;
    float redOffset, greenOffset, blueOffset, alphaOffset;
    XflColor() :
        redMultiplier(1), greenMultiplier(1), blueMultiplier(1), alphaMultiplier(1),
        redOffset(0), greenOffset(0), blueOffset(0), alphaOffset(0) {
    }
};

struct XflElement {
    std::string libraryItemName;
    std::string symbolType;
    XflMatrix matrix;
    XflColor color;
    float x, y;
    float scaleX, scaleY;
    float rotation;
    float alpha;
    int firstFrame;

    XflElement() : x(0), y(0), scaleX(1.0f), scaleY(1.0f), rotation(0), alpha(1.0f), firstFrame(0) {}
};

class XflParser {
public:
    bool LoadXflFile(const std::string& filename);
    const XflDOMDocument& GetDocument() const { return mDocument; }
    std::string GetBitmapPath(const std::string& bitmapName) const;
    bool GetElementTransform
        (const std::string& elementName, int frameIndex, XflElement& transform) const;
    ~XflParser();
private:
    bool ParseDOMDocument(const std::string& filepath);
    bool ParseSymbols(const std::string& libraryFolder);
    bool ParseBitmaps(const std::string& libraryFolder);
    bool ParseTimeline(const SimpleXmlNode& timelineNode, XflDOMTimeline& timeline);
    bool ParseLayer(const SimpleXmlNode& layerNode, XflDOMLayer& layer);
    bool ParseFrame(const SimpleXmlNode& frameNode, XflDOMFrame& frame);
    XflMatrix ParseMatrix(const std::string& matrixText);
    XflColor ParseColor(const std::string& colorText);   
    XflDOMDocument mDocument;
    std::string mBasePath;
};

#endif