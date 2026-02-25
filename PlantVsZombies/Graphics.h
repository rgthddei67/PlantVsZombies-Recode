#pragma once
#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/constants.hpp>  
#include <cmath>                 
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <mutex>
#include <functional>
#include <list>

#include "ShaderProgram.h"
#include "ResourceManager.h"

/**
 * @brief 将 glm::vec4 颜色转换为 SDL_Color。
 * @param color 输入颜色（各分量范围 [0,1]）
 * @return 转换后的 SDL_Color（各分量范围 0~255）
 */
inline SDL_Color ToSDLColor(const glm::vec4& color) {
	SDL_Color result;
	result.r = static_cast<Uint8>(color.r);
	result.g = static_cast<Uint8>(color.g);
	result.b = static_cast<Uint8>(color.b);
	result.a = static_cast<Uint8>(color.a);
	return result;
}

/**
 * @brief 批处理顶点格式（包含纹理坐标、纹理索引、矩阵索引和颜色）。
 */
struct BatchVertex {
	float x, y;          // 顶点位置（局部坐标，通常为 0~1 矩形）
	float u, v;          // 纹理坐标
	float texIndex;      // 纹理索引（对应纹理单元）
	float matrixIndex;   // 变换矩阵索引（对应矩阵数组）
	float r, g, b, a;    // 顶点颜色（预乘色调）
};

/**
 * @brief 几何图形顶点格式（仅位置和颜色，无纹理坐标）。
 */
struct GeomVertex {
	float x, y;          // 顶点位置（世界坐标）
	float r, g, b, a;    // 顶点颜色
};

/**
 * @brief 文字缓存条目，存储已生成的文字纹理及其尺寸。
 */
struct CachedText {
	GLuint textureID = 0;   // OpenGL 纹理 ID
	int width = 0;          // 纹理宽度（像素）
	int height = 0;         // 纹理高度（像素）
};

/**
 * @brief 混合模式
 */
enum class BlendMode {
	None,   // 不启用混合
	Alpha,  // 标准 Alpha 混合：GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
	Add     // 叠加混合：GL_SRC_ALPHA, GL_ONE
};

/**
 * @brief 图形渲染核心类，负责纹理绘制、文字渲染、几何图形绘制及批处理。
 *
 * 提供两种渲染模式：立即模式（直接调用 OpenGL）和批处理模式（自动合并绘制调用以提高性能）。
 * 支持变换矩阵栈、混合模式设置、文字缓存等功能。
 */
class Graphics {
public:
	/**
	 * @brief 构造函数，初始化变换栈并设置默认混合模式。
	 */
	Graphics();

	/**
	 * @brief 析构函数，释放所有 OpenGL 资源和缓存。
	 */
	~Graphics();

	/**
	 * @brief 初始化图形系统。
	 * @param windowWidth  窗口宽度（用于正交投影）
	 * @param windowHeight 窗口高度（用于正交投影）
	 * @return 初始化成功返回 true，否则 false
	 */
	bool Initialize(int windowWidth, int windowHeight);

	/**
	 * @brief 设置窗口尺寸，更新投影矩阵和视口。
	 * @param width  新宽度
	 * @param height 新高度
	 */
	void SetWindowSize(int width, int height);

	// ==================== 变换矩阵栈操作 ====================

	/**
	 * @brief 将给定矩阵乘以当前变换矩阵，并将结果压入栈顶。
	 * @param transform 要叠加的变换矩阵（默认为单位矩阵）
	 */
	void PushTransform(const glm::mat4& transform = glm::mat4(1.0f));

	/**
	 * @brief 弹出栈顶变换矩阵。若栈中只剩一个矩阵，则弹出失败并输出错误。
	 */
	void PopTransform();

	/**
	 * @brief 将当前变换矩阵设为单位矩阵。
	 */
	void SetIdentity();

	/**
	 * @brief 对当前变换矩阵应用平移变换。
	 * @param x X 轴平移量
	 * @param y Y 轴平移量
	 * @param z Z 轴平移量（默认为0）
	 */
	void Translate(float x, float y, float z = 0.0f);

	/**
	 * @brief 对当前变换矩阵应用旋转变换。
	 * @param angleDegrees 旋转角度（度）
	 * @param x            旋转轴 X 分量
	 * @param y            旋转轴 Y 分量
	 * @param z            旋转轴 Z 分量（默认为1，即绕Z轴）
	 */
	void Rotate(float angleDegrees, float x = 0.0f, float y = 0.0f, float z = 1.0f);

	/**
	 * @brief 对当前变换矩阵应用缩放变换。
	 * @param sx X 轴缩放系数
	 * @param sy Y 轴缩放系数
	 * @param sz Z 轴缩放系数（默认为1）
	 */
	void Scale(float sx, float sy, float sz = 1.0f);

	/**
	 * @brief 获取当前变换矩阵（栈顶矩阵）。
	 * @return 当前变换矩阵的常量引用
	 */
	const glm::mat4& GetCurrentTransform() const { return m_transformStack.back(); }

	// ==================== 绘制接口 ====================

	void DrawTexture(const GLTexture* tex, float x, float y, float width, float height,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(1.0f));

	/**
	 * @brief 绘制文字。
	 * @param text     文字内容
	 * @param fontKey  字体键名
	 * @param fontSize 字体大小
	 * @param color    文字颜色
	 * @param x        目标左上角 X 坐标
	 * @param y        目标左上角 Y 坐标
	 * @param scale    文字缩放系数（默认为1）
	 */
	void DrawText(const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale = 1.0f);

		/**
	 * @brief 使用自定义变换矩阵绘制纹理，并支持指定枢轴点。
	 * @param tex        纹理
	 * @param transform  模型变换矩阵（相对于纹理局部坐标）
	 * @param pivotX     枢轴点 X 坐标（纹理局部坐标，默认0）
	 * @param pivotY     枢轴点 Y 坐标（纹理局部坐标，默认0）
	 * @param tint       色调颜色
	 */
	void DrawTextureMatrix(const GLTexture* tex, const glm::mat4& transform,
		float pivotX = 0.0f, float pivotY = 0.0f,
		const glm::vec4& tint = glm::vec4(1.0f));

		/**
	 * @brief 绘制纹理的指定区域到目标矩形。
	 * @param tex    纹理指针
	 * @param srcX   源区域左上角 X（像素）
	 * @param srcY   源区域左上角 Y（像素）
	 * @param srcW   源区域宽度（像素）
	 * @param srcH   源区域高度（像素）
	 * @param dstX   目标矩形左上角 X
	 * @param dstY   目标矩形左上角 Y
	 * @param dstW   目标矩形宽度
	 * @param dstH   目标矩形高度
	 * @param rotation 旋转角度（度），绕目标矩形中心旋转
	 * @param tint   色调颜色
	 */
	void DrawTextureRegion(const GLTexture* tex,
		float srcX, float srcY, float srcW, float srcH,
		float dstX, float dstY, float dstW, float dstH,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(1.0f));

	// ==================== 几何图形绘制 ====================

	/**
	 * @brief 绘制线段。
	 * @param x1    起点 X 坐标
	 * @param y1    起点 Y 坐标
	 * @param x2    终点 X 坐标
	 * @param y2    终点 Y 坐标
	 * @param color 线段颜色（默认为白色）
	 */
	void DrawLine(float x1, float y1, float x2, float y2,
		const glm::vec4& color = glm::vec4(1.0f));

	/**
	 * @brief 绘制空心矩形（边框）。
	 * @param x      左上角 X 坐标
	 * @param y      左上角 Y 坐标
	 * @param width  宽度
	 * @param height 高度
	 * @param color  边框颜色（默认为白色）
	 */
	void DrawRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(1.0f));

	/**
	 * @brief 绘制实心矩形。
	 * @param x      左上角 X 坐标
	 * @param y      左上角 Y 坐标
	 * @param width  宽度
	 * @param height 高度
	 * @param color  填充颜色（默认为白色）
	 */
	void FillRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(1.0f));

	/**
	 * @brief 绘制空心圆（折线近似）。
	 * @param cx       圆心 X 坐标
	 * @param cy       圆心 Y 坐标
	 * @param radius   半径
	 * @param color    边框颜色（默认为白色）
	 * @param segments 分段数（越多越圆滑，默认32）
	 */
	void DrawCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(1.0f), int segments = 32);

	/**
	 * @brief 绘制实心圆（扇形三角形）。
	 * @param cx       圆心 X 坐标
	 * @param cy       圆心 Y 坐标
	 * @param radius   半径
	 * @param color    填充颜色（默认为白色）
	 * @param segments 分段数（越多越圆滑，默认32）
	 */
	void FillCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(1.0f), int segments = 32);

	// ==================== 批处理控制 ====================

	/**
	 * @brief 开始批处理（无实际操作，批处理始终累积，直到 FlushBatch 被调用）。
	 */
	void BeginBatch();

	/**
	 * @brief 结束批处理并提交所有累积的绘制命令。
	 */
	void EndBatch();

	/**
	 * @brief 提交当前批处理缓冲区中的所有顶点，并清空相关容器。
	 */
	void FlushBatch();

	/**
	 * @brief 设置批处理模式开关。切换时会自动提交当前批次。
	 * @param enable true 启用批处理，false 禁用（立即模式）
	 */
	void SetBatchMode(bool enable);

	// ==================== 文字缓存管理 ====================

	/**
	 * @brief 清除文字缓存，删除所有缓存的文字纹理。
	 */
	void ClearTextCache();

	// ==================== 渲染状态设置 ====================

	/**
	 * @brief 设置清屏颜色。
	 * @param r 红色分量 [0,1]
	 * @param g 绿色分量 [0,1]
	 * @param b 蓝色分量 [0,1]
	 * @param a Alpha 分量 [0,1]
	 */
	void SetClearColor(float r, float g, float b, float a);

	/**
	 * @brief 设置混合模式。
	 * @param mode 混合模式枚举值
	 */
	void SetBlendMode(BlendMode mode);

	/**
	 * @brief 获取当前混合模式。
	 * @return 当前混合模式
	 */
	BlendMode GetBlendMode() const { return m_currentBlendMode; }

	/**
	 * @brief 清除颜色缓冲和深度缓冲。
	 */
	void Clear();


	/**
	 * @brief 设置摄像机位置（世界坐标）。
	 */
	void SetCameraPosition(float x, float y);

	/**
	 * @brief 移动摄像机（相对偏移）。
	 */
	void MoveCamera(float dx, float dy);

	/**
	 * @brief 设置摄像机缩放倍率（默认1.0）。
	 */
	void SetCameraZoom(float zoom);

	/**
	 * @brief 设置摄像机旋转角度（度）。
	 */
	void SetCameraRotation(float degrees);

	glm::vec2 GetCameraPosition() const { return m_cameraPos; }
	float GetCameraZoom()         const { return m_cameraZoom; }
	float GetCameraRotation()     const { return m_cameraRotation; }

	/**
	 * @brief 重置摄像机到默认状态（位置归零、缩放1、旋转0）。
	 */
	void ResetCamera();

	/**
	 * @brief 将屏幕坐标转换为世界坐标（考虑摄像机位移、缩放、旋转）。
	 * @param screenX 屏幕 X 坐标（像素）
	 * @param screenY 屏幕 Y 坐标（像素）
	 * @return 对应的世界坐标
	 */
	glm::vec2 ScreenToWorldPosition(float screenX, float screenY) const;

	/**
	 * @brief 将世界坐标转换为屏幕坐标。
	 * @param worldX 世界 X 坐标
	 * @param worldY 世界 Y 坐标
	 * @return 对应的屏幕坐标（像素）
	 */
	glm::vec2 WorldToScreenPosition(float worldX, float worldY) const;

	// ==================== 多线程渲染支持 ====================

	/**
	 * @brief 从任意线程提交一个渲染命令（lambda）。
	 *        命令将在下一次 ProcessCommandQueue() 时由渲染线程执行。
	 * @param cmd 渲染命令（可捕获任意参数的 lambda）
	 */
	void Submit(std::function<void()> cmd);

	/**
	 * @brief 处理所有待执行的渲染命令队列（必须在渲染线程调用）。
	 *        通常在每帧 Clear() 之后、FlushBatch() 之前调用。
	 */
	void ProcessCommandQueue();

	// 以下为线程安全的绘制提交接口，可从任意线程调用
	void SubmitDrawTexture(const GLTexture* texture, float x, float y, float width, float height,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(1.0f));

	void SubmitDrawTextureMatrix(const GLTexture* texture, const glm::mat4& transform,
		float pivotX = 0.0f, float pivotY = 0.0f,
		const glm::vec4& tint = glm::vec4(1.0f));

	void SubmitDrawTextureRegion(const GLTexture* tex,
		float srcX, float srcY, float srcW, float srcH,
		float dstX, float dstY, float dstW, float dstH,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(1.0f));

	void SubmitDrawText(const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale = 1.0f);

	void SubmitDrawLine(float x1, float y1, float x2, float y2,
		const glm::vec4& color = glm::vec4(1.0f));

	void SubmitDrawRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(1.0f));

	void SubmitFillRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(1.0f));

	void SubmitDrawCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(1.0f), int segments = 32);

	void SubmitFillCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(1.0f), int segments = 32);

private:
	BlendMode m_currentBlendMode = BlendMode::None;   ///< 当前混合模式

	ShaderProgram m_spriteShader;      ///< 精灵着色器（立即模式）
	ShaderProgram m_batchShader;        ///< 批处理着色器
	ShaderProgram m_geomShader;         ///< 几何图形着色器（仅颜色，无纹理）
	GLuint m_spriteVAO = 0;              ///< 精灵渲染 VAO
	GLuint m_spriteVBO = 0;              ///< 精灵渲染 VBO
	GLuint m_spriteEBO = 0;              ///< 精灵渲染 EBO

	glm::mat4 m_projection = glm::mat4(1.0f);   ///< 正交投影矩阵
	glm::mat4 m_viewMatrix = glm::mat4(1.0f);   ///< 摄像机视图矩阵
	int m_windowWidth = 0;                      ///< 窗口宽度（像素）
	int m_windowHeight = 0;                      ///< 窗口高度（像素）

	// 摄像机状态
	glm::vec2 m_cameraPos = glm::vec2(0.0f);  ///< 摄像机世界坐标
	float     m_cameraZoom = 1.0f;              ///< 缩放倍率
	float     m_cameraRotation = 0.0f;              ///< 旋转角度（度）

	std::vector<glm::mat4> m_transformStack;   ///< 变换矩阵栈

	// 批处理数据缓冲区
	std::vector<BatchVertex> m_batchVertices;   ///< 批处理顶点列表
	std::vector<GLuint> m_batchTextures;        ///< 当前批次使用的纹理 ID 列表
	std::vector<glm::mat4> m_batchMatrices;     ///< 当前批次使用的变换矩阵列表

	int m_maxTextureUnits = 32;                  ///< 最大纹理单元数（着色器限制）

	GLuint m_batchVAO = 0;                       ///< 批处理 VAO
	GLuint m_batchVBO = 0;                       ///< 批处理 VBO
	GLuint m_matrixUBO = 0;                      ///< 矩阵 UBO（替代 uniform 数组，突破 64 上限）
	size_t m_batchBufferCapacity = 0;             ///< 当前 VBO 容量（顶点个数）

	GLuint m_geomVAO = 0;                         ///< 几何图形 VAO
	GLuint m_geomVBO = 0;                         ///< 几何图形 VBO

	std::vector<GeomVertex> m_geomBatchVertices;  ///< 几何图形批处理顶点列表
	GLenum m_geomBatchMode = GL_TRIANGLES;         ///< 当前几何批处理图元类型（仅 GL_LINES / GL_TRIANGLES）
	size_t m_geomBatchCapacity = 0;                ///< 几何批处理 VBO 容量（顶点个数）

	bool m_batchMode = true;                      ///< 是否启用批处理模式

	static const int VERTEX_BATCH_LIMIT = 1024;   ///< 单批次最大顶点数
	static const int MATRIX_BATCH_LIMIT = 256;    ///< 单批次最大矩阵数（UBO 保证最小 16KB，可存 256 个 mat4）
	static const int GEOM_BATCH_LIMIT   = 2048;   ///< 单批次最大几何顶点数

	static const int TEXT_CACHE_MAX_SIZE = 256;  ///< 文字缓存最大条目数（LRU 淘汰）
	std::list<std::string> m_textCacheOrder;     ///< LRU 顺序链表（front = 最近使用）
	std::unordered_map<std::string,
		std::pair<CachedText, std::list<std::string>::iterator>> m_textCache;  ///< 文字纹理 LRU 缓存

	// 多线程命令队列
	std::mutex m_commandMutex;                               ///< 命令队列互斥锁
	std::vector<std::function<void()>> m_pendingCommands;    ///< 待处理命令（任意线程写入）
	std::vector<std::function<void()>> m_activeCommands;     ///< 当前帧处理命令（渲染线程读取）

	// ==================== 内部辅助函数 ====================

	/**
	 * @brief 加载并编译着色器。
	 * @return 成功返回 true
	 */
	bool InitShaders();

	/**
	 * @brief 根据摄像机状态重新计算视图矩阵。
	 */
	void UpdateViewMatrix();

	/**
	 * @brief 初始化精灵渲染器（立即模式）。
	 * @return 成功返回 true
	 */
	bool InitSpriteRenderer();

	/**
	 * @brief 初始化几何图形渲染器。
	 * @return 成功返回 true
	 */
	bool InitGeomRenderer();

	/**
	 * @brief 立即模式绘制几何图形（内部使用）。
	 * @param vertices 顶点数据
	 * @param mode     OpenGL 图元类型
	 */
	void DrawGeomImmediate(const std::vector<GeomVertex>& vertices, GLenum mode);

	/**
	 * @brief 提交几何批处理缓冲区中的所有顶点并清空。
	 */
	void FlushGeomBatch();

	/**
	 * @brief 将纹理 ID 绑定到批处理纹理列表，返回纹理单元索引。
	 * @param textureID OpenGL 纹理 ID
	 * @return 纹理单元索引
	 */
	int BindTexture(GLuint textureID);

	/**
	 * @brief 将变换矩阵添加到批处理矩阵列表，返回矩阵索引。
	 * @param matrix 变换矩阵
	 * @return 矩阵索引
	 */
	int AddMatrix(const glm::mat4& matrix);

	/**
	 * @brief 向批处理顶点列表中添加多个顶点。
	 * @param vertices 顶点数组指针
	 * @param count    顶点数量
	 */
	void AddVertices(const BatchVertex* vertices, int count);

	/**
	 * @brief 检查批处理缓冲区是否达到限制，若达到则刷新。
	 */
	void CheckBatch();

	/**
	 * @brief 获取或创建文字纹理。如果文字已缓存，则直接返回缓存的纹理ID。
	 * @param text      文字内容
	 * @param fontKey   字体键名
	 * @param fontSize  字体大小
	 * @param color     文字颜色
	 * @param outWidth  输出纹理宽度
	 * @param outHeight 输出纹理高度
	 * @return OpenGL 纹理 ID，失败返回 0
	 */
	GLuint GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,
		int fontSize, const glm::vec4& color,
		int& outWidth, int& outHeight);

	/**
	 * @brief 调整批处理 VBO 的容量。
	 * @param newCapacity 新容量（顶点个数）
	 */
	void ResizeBatchBuffer(size_t newCapacity);
};

#endif