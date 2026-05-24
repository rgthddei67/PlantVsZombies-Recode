#pragma once
#ifndef __GRAPHICS_H__
#define __GRAPHICS_H__

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
#include <list>
#include <memory>
#include <cstdint>

#include "ResourceManager.h"

namespace pvz {
    class VulkanContext;
    class VulkanRenderer;
    class VulkanTexturePool;
    struct VulkanTexture;
}

/**
 * @brief 将 glm::vec4 颜色转换为 SDL_Color。
 * @param color 输入颜色（各分量范围 [0,255]）
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
 * @brief 将 0-255 格式的颜色转换为 0-1 归一化格式（用于 OpenGL）。
 * @param color 输入颜色（各分量范围 [0,255]）
 * @return 归一化后的颜色（各分量范围 [0,1]）
 */
inline glm::vec4 NormalizeColor(const glm::vec4& color) {
	return glm::vec4(color.r / 255.0f, color.g / 255.0f,
		color.b / 255.0f, color.a / 255.0f);
}

/**
 * @brief 批处理顶点格式（包含纹理坐标、纹理索引、矩阵索引和颜色）。
 */
struct BatchVertex {
	float x, y;          // 顶点位置（局部坐标，通常为 0~1 矩形）
	float u, v;          // 纹理坐标
	uint32_t texIndex;     // 纹理索引（bindless 槽位）
	uint32_t matrixIndex;  // 变换矩阵索引（SSBO 槽位）
	float r, g, b, a;    // 顶点颜色（预乘色调）
	float blendMode;     // 混合模式（0.0 = Alpha, 1.0 = Additive），仅 CPU 侧分段使用
};

/**
 * @brief Per-sprite instance record consumed by reanim_inst.vert.glsl.
 *
 *        Vertex shader expands gl_VertexIndex 0..5 to a unit-quad corner+UV,
 *        applies the 2x3 affine (columns (tA,tB) and (tC,tD); translation (tx,ty)),
 *        samples atlas UV range (u0,v0,u1,v1), multiplies vertex color.
 *
 *        CPU side (Task 5) must pre-multiply tA..tD by (sprite_width × Scale) and
 *        (sprite_height × Scale) so the shader's `corner` is a unit quad.
 *
 *        Stride 48 B (16-byte aligned for std140-style SSBO layout).
 */
struct InstanceRecord {
    float tA, tB, tC, tD;   // 16 B — pre-multiplied 2x2: cols (tA,tB) and (tC,tD)
    float tx, ty;           // 8 B  — world translation (already absorbs baseX + transform.x*Scale + offsets)
    float u0, v0;           // 8 B  — atlas UV top-left
    float u1, v1;           // 8 B  — atlas UV bottom-right
    uint32_t texSlot;       // 4 B  — bindless texture index
    uint32_t colorRGBA8;    // 4 B  — packed RGBA8 (r=lsb, a=msb), pre-tinted
};
static_assert(sizeof(InstanceRecord) == 48, "InstanceRecord must be 48 bytes");

/**
 * @brief 文字缓存条目，存储已生成的文字纹理及其尺寸。
 *        Phase 3c：textureID 现在是 bindless 槽位下标；vkTex 持有上传到 VulkanTexturePool
 *        的纹理句柄，LRU 淘汰 / 缓存清空时用它把槽位还回 pool。
 */
struct CachedText {
	uint32_t textureID = 0;          // bindless 槽位（Phase 3c）
	int width = 0;
	int height = 0;
	pvz::VulkanTexture* vkTex = nullptr;
};

/**
 * @brief 屏幕像素裁剪矩形（左上原点，与 DrawTexture 等绘制接口的坐标语义一致）。
 *        宽或高 <= 0 视为退化矩形，等价于"什么都不画"。
 */
struct ClipRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};

/**
 * @brief 混合模式
 */
enum class BlendMode {
	None,   // 不启用混合
	Alpha,  // 标准 Alpha 混合：GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA
	Add     // 叠加混合：GL_SRC_ALPHA, GL_ONE
};

// ==================== 多线程录制（Record / Replay）相关结构 ====================
//
// 设计：GameObjectManager 在 Draw 阶段把对象按渲染顺序切成 N 块，每个工作线程把自己
// 那块的 Graphics 调用录制到 thread-local 的 WorkerRecord（POD 命令流 + 顶点池），
// 不调任何 GL；主线程随后按 slot 顺序回放，转成真实的 BindTexture / AddMatrix /
// AddVertices / FlushBatch 调用。这样把矩阵乘法、UV、顶点构造等纯 CPU 工作分摊到多
// 核，主线程仅做 GL 调用与轻量 append。绘制顺序通过 slot 升序 + slot 内 record 顺
// 序严格保留。

/**
 * @brief 录制命令类型。命令类型决定如何在回放时解释 RecordCmd 的字段。
 *        Phase 5：BatchVerts 已删除——worker 直接把顶点和矩阵写进 VkWorkerSlice 指向的
 *        持久映射 VBO/SSBO 切片，cmds 仅用于记录状态变更点（让回放知道在哪里切 vkCmdDraw）。
 */
enum class RecCmdType : uint8_t {
	PushClip,       ///< 推入裁剪矩形（payloadIdx 指向 clips）
	PopClip,        ///< 弹出裁剪矩形
	SetBlend,       ///< 切换混合模式（payloadIdx 指向 blendModes）
	DeferredText    ///< DrawText 延迟到主线程执行（payloadIdx 指向 textCmds）
};

/**
 * @brief 单条录制命令（16 字节，POD）。Phase 5 简化：只标记状态变更点。
 *        vertOffsetAtCmd 是命令插入时 slice.vboCount 的值，回放时配合 slice.vboBaseVert
 *        定位状态变更应该发生在哪个绝对顶点位置，并据此切分 vkCmdDraw 区段。
 *        instOffsetAtCmd 是命令插入时 slice.instCount 的值（Task 4），用于切分实例 draw 区段。
 */
struct RecordCmd {
	RecCmdType type;             ///< 1 B
	uint8_t    pad0 = 0;         ///< 1 B  对齐填充
	uint16_t   pad1 = 0;         ///< 2 B  对齐填充
	uint32_t   vertOffsetAtCmd;  ///< 4 B  命令插入时 slice.vboCount
	uint32_t   instOffsetAtCmd;  ///< 4 B  命令插入时 slice.instCount（Task 4）
	uint32_t   payloadIdx;       ///< 4 B  clips/blendModes/textCmds 下标
};
static_assert(sizeof(RecordCmd) == 16, "RecordCmd should pack to 16 bytes");

/**
 * @brief 延迟执行的 DrawText 参数。worker 不能触碰 LRU 与纹理上传，所以把整
 *        条 DrawText 调用打包，主线程回放时再走原 DrawText 路径。
 */
struct DeferredTextCmd {
	std::string text;
	std::string fontKey;
	int         fontSize = 0;
	glm::vec4   color = glm::vec4(255.0f);
	float       x = 0.0f;
	float       y = 0.0f;
	float       scale = 1.0f;
};

/**
 * @brief Phase 5：每个 worker 在当前帧持久映射 VBO/SSBO 中的非重叠窗口。
 *        BeginParallelRecord 切分容量后填充指针 / 基址 / 容量，worker 线程独占写入。
 *        vboBaseVert / ssboBaseMat 是 vboPtr[0] / ssboPtr[0] 在整帧缓冲里的绝对元素
 *        下标——worker 写顶点时直接把 matrixIndex 设为 (ssboBaseMat + ssboCount)，
 *        replay 不再做任何 patch；texIndex 也是 bindless 槽位的绝对值。
 */
struct VkWorkerSlice {
	BatchVertex*    vboPtr      = nullptr;  ///< 切片首个 BatchVertex 的映射指针
	glm::mat4*      ssboPtr     = nullptr;  ///< 切片首个 mat4 的映射指针
	InstanceRecord* instPtr     = nullptr;  ///< 切片首个 InstanceRecord 的映射指针（Task 3）
	uint32_t        vboBaseVert = 0;        ///< vboPtr[0] 在整帧 VBO 中的绝对顶点下标
	uint32_t        ssboBaseMat = 0;        ///< ssboPtr[0] 在整帧 SSBO 中的绝对 mat4 下标
	uint32_t        instBaseIdx = 0;        ///< instPtr[0] 在整帧 instBuf 中的绝对下标（Task 3）
	uint32_t        vboCap      = 0;        ///< 切片容量（BatchVertex 个数）
	uint32_t        ssboCap     = 0;        ///< 切片容量（mat4 个数）
	uint32_t        instCap     = 0;        ///< 切片容量（InstanceRecord 个数）（Task 3）
	uint32_t        vboCount    = 0;        ///< 当前已写顶点数
	uint32_t        ssboCount   = 0;        ///< 当前已写矩阵数
	uint32_t        instCount   = 0;        ///< 当前已写 InstanceRecord 数（Task 3）
	bool            overflowed  = false;    ///< 写超容量时置 true，回放只画前 vboCount 个
};

/**
 * @brief 每个 worker slot 一份的录制缓冲。Phase 5 起 verts/texList/matList 已删除——
 *        顶点和矩阵走 slice 直写 VBO/SSBO；cmds 只记录状态变更（clip/blend/text）。
 *        析构无 GL 资源，Reset 仅 clear()，capacity 跨帧复用以避免 realloc。
 */
struct WorkerRecord {
	VkWorkerSlice                slice;          ///< Phase 5：本帧的 VBO/SSBO 切片
	std::vector<RecordCmd>       cmds;           ///< 仅状态变更命令流
	std::vector<ClipRect>        clips;          ///< PushClip 的 payload
	std::vector<BlendMode>       blendModes;     ///< SetBlend 的 payload
	std::vector<DeferredTextCmd> textCmds;       ///< DeferredText 的 payload

	// 初始状态快照（BeginParallelRecord 时由主线程填充，SetWorkerSlot 时给 worker 用）
	glm::mat4              initialTopTransform   = glm::mat4(1.0f);
	bool                   initialTopIsIdentity  = true;
	std::vector<ClipRect>  initialClipStack;
	BlendMode              initialBlend          = BlendMode::None;

	// 清空所有命令，但保留 vector capacity，下帧复用避免 realloc。
	// slice 不在这里重置——由 BeginParallelRecord 每帧重新切分并写入。
	void Reset() {
		cmds.clear();
		clips.clear();
		blendModes.clear();
		textCmds.clear();
		initialClipStack.clear();
	}
};

/**
 * @brief 每个 worker slot 一份的 thread-local 栈存储。SetWorkerSlot 时让 worker 的
 *        thread_local 指针指向这里，clip / transform 栈在这里增删，结束时不释放。
 */
struct WorkerThreadState {
	std::vector<glm::mat4> transformStack;
	std::vector<char>      transformIsIdentity;
	std::vector<ClipRect>  clipStack;
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

	// Phase 3b — Vulkan 渲染接入。
	// InitializeVulkan: 在 VulkanContext / VulkanRenderer / VulkanTexturePool 都就绪、
	//   且所有纹理已经上传之后调用一次，建立 batch pipeline、逐帧 VBO/SSBO、descriptor set。
	// BeginFrame / EndFrame: GameApp::Draw 每帧调用一对，承担 acquire→record→submit→present。
	//   绘制 API（DrawTexture 等）必须在这两个调用之间使用。
	bool InitializeVulkan(pvz::VulkanContext* ctx,
	                      pvz::VulkanRenderer* renderer,
	                      pvz::VulkanTexturePool* pool);
	void ShutdownVulkan();
	bool BeginFrame();
	bool EndFrame();

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

	// ==================== 裁剪栈（屏幕像素矩形，类似 Unity RectMask2D） ====================

	/**
	 * @brief 压入一个屏幕像素矩形作为当前裁剪区域；区域外的像素不会被绘制。
	 *        坐标采用屏幕左上原点（与 DrawTexture 等接口一致），不被 Transform 栈或摄像机影响。
	 *        嵌套调用时与父矩形取交集 —— 子裁剪不能"暴露"父裁剪遮住的内容。
	 *        会强制刷新当前批处理（否则之前累积的顶点会被新 scissor 一起裁剪），所以应当
	 *        粗粒度使用（在一组绘制外面 push 一次），不要在每个 GameObject 周围 push/pop。
	 * @param x 矩形左上角 X（屏幕像素）
	 * @param y 矩形左上角 Y（屏幕像素）
	 * @param w 矩形宽度（屏幕像素）
	 * @param h 矩形高度（屏幕像素）
	 */
	void PushClipRect(int x, int y, int w, int h);

	/**
	 * @brief 弹出栈顶裁剪矩形，恢复到父矩形（若栈空则禁用 scissor test）。
	 *        会强制刷新当前批处理。栈空时调用会输出错误并直接返回。
	 */
	void PopClipRect();

	/**
	 * @brief 当前是否有生效的裁剪矩形。
	 */
	bool IsClipActive() const { return !m_clipStack.empty(); }

	/**
	 * @brief 取得当前生效的裁剪矩形（栈顶，已经过嵌套交集）。
	 * @return 栈空返回 nullptr。
	 */
	const ClipRect* CurrentClipRect() const {
		return m_clipStack.empty() ? nullptr : &m_clipStack.back();
	}

	// ==================== 绘制接口 ====================

	void DrawTexture(const Texture* tex, float x, float y, float width, float height,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(255.0f));

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
	 * @brief 取得一份常驻（pinned）文字纹理句柄；颜色已烘焙到纹理。
	 *        用于对同一段文字频繁重绘的场景，避免 DrawText 每帧的 key 构造与 LRU 维护开销。
	 *        句柄直到 Graphics 销毁（或显式清理）前都有效，不参与 LRU 淘汰。
	 * @return 句柄；若渲染失败 textureID 为 0。
	 */
	CachedText AcquireTextTexture(const std::string& text, const std::string& fontKey,
		int fontSize, const glm::vec4& color);

	/**
	 * @brief 使用 AcquireTextTexture 返回的句柄直接批次提交，跳过缓存查找。
	 */
	void DrawCachedText(const CachedText& handle, float x, float y, float scale = 1.0f);

	/**
 * @brief 使用自定义变换矩阵绘制纹理，并支持指定枢轴点。
 * @param tex        纹理
 * @param transform  模型变换矩阵（相对于纹理局部坐标）
 * @param pivotX     枢轴点 X 坐标（纹理局部坐标，默认0）
 * @param pivotY     枢轴点 Y 坐标（纹理局部坐标，默认0）
 * @param tint       色调颜色
 */
	void DrawTextureMatrix(const Texture* tex, const glm::mat4& transform,
		float pivotX = 0.0f, float pivotY = 0.0f,
		const glm::vec4& tint = glm::vec4(255.0f),
		BlendMode blendMode = BlendMode::None);

	/**
	 * @brief Append a per-sprite instance record consumed by the GPU-instancing reanim pipeline.
	 *        Caller (Task 5+) must pre-multiply tA..tD by (sprite_width × Scale) /
	 *        (sprite_height × Scale); tx/ty are absolute world coords; tex slot is a bindless
	 *        index; colorRGBA8 is pre-tinted (r=lsb, a=msb).
	 *
	 *        Worker thread (tl_record set): writes into the slot's slice.instPtr at instCount,
	 *        records a SetBlend cmd if blendMode differs from tl_blend.
	 *        Main thread: appends to m_batchInstances, flushes on BlendMode change or chunk full.
	 */
	void AppendReanimInstance(const InstanceRecord& rec, BlendMode blendMode);

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
	void DrawTextureRegion(const Texture* tex,
		float srcX, float srcY, float srcW, float srcH,
		float dstX, float dstY, float dstW, float dstH,
		float rotation = 0.0f, const glm::vec4& tint = glm::vec4(255.0f));

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
		const glm::vec4& color = glm::vec4(255.0f));

	/**
	 * @brief 绘制空心矩形（边框）。
	 * @param x      左上角 X 坐标
	 * @param y      左上角 Y 坐标
	 * @param width  宽度
	 * @param height 高度
	 * @param color  边框颜色（默认为白色）
	 */
	void DrawRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(255.0f));

	/**
	 * @brief 绘制实心矩形。
	 * @param x      左上角 X 坐标
	 * @param y      左上角 Y 坐标
	 * @param width  宽度
	 * @param height 高度
	 * @param color  填充颜色（默认为白色）
	 */
	void FillRect(float x, float y, float width, float height,
		const glm::vec4& color = glm::vec4(255.0f));

	/**
	 * @brief 绘制空心圆（折线近似）。
	 * @param cx       圆心 X 坐标
	 * @param cy       圆心 Y 坐标
	 * @param radius   半径
	 * @param color    边框颜色（默认为白色）
	 * @param segments 分段数（越多越圆滑，默认32）
	 */
	void DrawCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(255.0f), int segments = 32);

	/**
	 * @brief 绘制实心圆（扇形三角形）。
	 * @param cx       圆心 X 坐标
	 * @param cy       圆心 Y 坐标
	 * @param radius   半径
	 * @param color    填充颜色（默认为白色）
	 * @param segments 分段数（越多越圆滑，默认32）
	 */
	void FillCircle(float cx, float cy, float radius,
		const glm::vec4& color = glm::vec4(255.0f), int segments = 32);

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
	 * @param r 红色分量 [0,255]
	 * @param g 绿色分量 [0,255]
	 * @param b 蓝色分量 [0,255]
	 * @param a Alpha 分量 [0,255]
	 */
	void SetClearColor(float r, float g, float b, float a);

	/**
	 * @brief 获取当前清屏颜色（归一化 [0,1]）。Phase 3a：由 VulkanRenderer 在每帧 BeginFrame 时使用。
	 */
	void GetClearColor(float& r, float& g, float& b, float& a) const {
		r = m_clearR; g = m_clearG; b = m_clearB; a = m_clearA;
	}

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
	 * @brief Task 7: A/B toggle for the GPU-instancing reanim path.
	 *        Default true. When false, Animator::DrawInternal forces the slow
	 *        DrawTextureMatrix path even for animators without children.
	 *        Set at startup via main.cpp's -NoInstance flag.
	 */
	void SetInstancePathEnabled(bool e) { m_useInstancePath = e; }
	bool IsInstancePathEnabled() const { return m_useInstancePath; }

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

	// ==================== 多线程录制 / 回放（Record / Replay） ====================
	//
	// 用法（GameObjectManager::DrawAll 内）：
	//   g->BeginParallelRecord(N);                          // 主线程
	//   threadPool->Dispatch(total, [](int s, int e){
	//       g->SetWorkerSlot(slot);
	//       for (i in [s,e)) { ... obj->Draw(g); ... }
	//       g->ClearWorkerSlot();
	//   });
	//   g->ReplayAndEndParallel();                          // 主线程
	//
	// worker 内调用 DrawTexture / DrawTextureMatrix / DrawText / PushTransform 等
	// 都会被 Graphics 内部 thread_local 检测拦截到 record 路径，不调任何 GL。

	/**
	 * @brief 主线程：开启并行录制阶段，确保有 numWorkers 个 WorkerRecord，
	 *        Reset 每个 record 并把当前 Graphics 状态（变换栈顶、裁剪栈、混合模式）
	 *        快照到每个 record 的 initial* 字段。首次调用会按推荐容量预分配以避
	 *        免运行中 realloc。
	 */
	void BeginParallelRecord(int numWorkers);

	/**
	 * @brief worker 线程：把本线程的 thread_local 指针指向 slot 对应的 WorkerRecord
	 *        与 WorkerThreadState；用 record 的 initial* 字段初始化 thread-local 的
	 *        变换栈与裁剪栈。slot 必须在 [0, numWorkers)。
	 */
	void SetWorkerSlot(int slot);

	/**
	 * @brief worker 线程：清空 thread_local 指针。底层存储不释放，下帧复用 capacity。
	 *        若发现 worker 块结束时栈不平衡，会打印警告（防御性）。
	 */
	void ClearWorkerSlot();

	/**
	 * @brief 主线程：按 slot 0..N-1 顺序回放所有录制的命令到真实 Graphics 状态，
	 *        触发必要的 FlushBatch / GL 调用，最后重置标志。不释放 record 存储。
	 */
	void ReplayAndEndParallel();

	/// 运行时开关：true 启用并行 record/replay 路径，false 强制走原串行路径（A/B 测试用）。
	void SetParallelDrawEnabled(bool e) { m_parallelDrawEnabled = e; }
	bool IsParallelDrawEnabled() const { return m_parallelDrawEnabled; }

private:
	BlendMode m_currentBlendMode = BlendMode::None;   ///< 当前混合模式

	glm::mat4 m_projection = glm::mat4(1.0f);   ///< 正交投影矩阵
	glm::mat4 m_viewMatrix = glm::mat4(1.0f);   ///< 摄像机视图矩阵
	int m_windowWidth = 0;                      ///< 窗口宽度（像素）
	int m_windowHeight = 0;                      ///< 窗口高度（像素）

	// 摄像机状态
	glm::vec2 m_cameraPos = glm::vec2(0.0f);  ///< 摄像机世界坐标
	float     m_cameraZoom = 1.0f;              ///< 缩放倍率
	float     m_cameraRotation = 0.0f;              ///< 旋转角度（度）

	std::vector<glm::mat4> m_transformStack;   ///< 变换矩阵栈
	std::vector<char>      m_transformIsIdentity; ///< 与 m_transformStack 平行：该层是否为单位阵（用于跳过冗余 mat4 乘法）

	std::vector<ClipRect> m_clipStack;          ///< 裁剪矩形栈（屏幕像素，已经过嵌套交集）

	// 批处理数据缓冲区
	std::vector<BatchVertex> m_batchVertices;   ///< 批处理顶点列表
	std::vector<uint32_t> m_batchTextures;        ///< 当前批次使用的 bindless 槽位列表
	std::vector<glm::mat4> m_batchMatrices;     ///< 当前批次使用的变换矩阵列表
	std::vector<InstanceRecord> m_batchInstances;   ///< 主线程串行 instance 缓冲（worker 走 slice 不经此处）
	int m_batchInstancesLimit = 8192;               ///< 单次 flush 上限，~384 KB 一次 vkCmdDraw
	bool m_useInstancePath = true;   ///< Task 7: false强制走 slow path 做 A/B baseline

	int m_maxTextureUnits = 32;                  ///< 最大纹理单元数（着色器限制）
	int m_matrixBatchLimit = 256;                ///< 单批次最大矩阵数（运行时由 GL_MAX_UNIFORM_BLOCK_SIZE 决定，最低 256）

	size_t m_batchBufferCapacity = 0;             ///< 当前 VBO 容量（顶点个数）

	bool m_batchMode = true;                      ///< 是否启用批处理模式

	uint32_t m_whiteTexture = 0;   ///< 1×1 纯白纹理 bindless 槽位，用于 FillRect 批处理

	// Phase 3a：清屏颜色（归一化 [0,1]），SetClearColor 写入、GetClearColor 读出，供 VulkanRenderer 使用。
	float m_clearR = 1.0f, m_clearG = 1.0f, m_clearB = 1.0f, m_clearA = 1.0f;

	// Phase 3b：Vulkan 真渲染的 PIMPL 状态（pipeline / 逐帧 VBO+SSBO / descriptor set 等），
	// 定义在 Graphics.cpp 内部，避免把 vulkan.h 暴露给整个工程。
	struct VulkanGraphicsState;
	std::unique_ptr<VulkanGraphicsState> m_vk;

	static const int VERTEX_BATCH_LIMIT_MIN = 1024;   ///< 单批次最大顶点数最低保证（运行时可更大）
	int m_vertexBatchLimit = VERTEX_BATCH_LIMIT_MIN;  ///< 单批次最大顶点数（= m_matrixBatchLimit * 6）
	static const int MATRIX_BATCH_LIMIT_MIN = 256;    ///< 单批次最大矩阵数最低保证（UBO 保证最小 16KB）
	static const int GEOM_BATCH_LIMIT = 2048;   ///< 单批次最大几何顶点数

	static const int TEXT_CACHE_MAX_SIZE = 256;  ///< 文字缓存最大条目数（LRU 淘汰）
	std::list<std::string> m_textCacheOrder;     ///< LRU 顺序链表（front = 最近使用）
	std::unordered_map<std::string,
		std::pair<CachedText, std::list<std::string>::iterator>> m_textCache;  ///< 文字纹理 LRU 缓存
	std::unordered_map<std::string, CachedText> m_pinnedTextCache;  ///< 常驻文字纹理缓存（AcquireTextTexture 使用，不淘汰）

	// ==================== 多线程录制状态 ====================
	std::vector<WorkerRecord>      m_workerRecords;       ///< 每个 worker slot 一份的录制缓冲
	std::vector<WorkerThreadState> m_workerStates;        ///< 每个 worker slot 一份的栈存储
	int                            m_numActiveWorkers = 0;///< 本轮 Dispatch 的有效 slot 数（=Replay 时遍历的上限）
	bool                           m_parallelDrawEnabled = true; ///< 是否启用并行 record/replay 路径

	// Phase 5：BeginParallelRecord 切片时记录主线程基准游标和切片总占用，
	// ReplayAndEndParallel 收尾时据此跳过整段并行区域。
	// 用 uint64_t 而非 VkDeviceSize 是为了避免在 Graphics.h 里引入 vulkan.h（保持 PIMPL）。
	uint64_t m_parallelBaseVbo   = 0;
	uint64_t m_parallelBaseSsbo  = 0;
	uint64_t m_parallelBaseInst  = 0;
	uint64_t m_parallelVboBytes  = 0;
	uint64_t m_parallelSsboBytes = 0;
	uint64_t m_parallelInstBytes = 0;

	// 自适应负载均衡（修复 worker 切片单点溢出）：
	// 并行区不再按 worker 数等分——按"上一并行帧各 slot 的实际占用"成比例切片，
	// 重的 slot 下帧拿更大切片、轻的缩小，总和 ≤ 可用区。溢出的 slot 真实需求
	// 被丢弃无法测量，记为 cap×1.5 的放大估计，使其 1~2 帧内收敛（快 attack）。
	// slot→对象映射与回放契约保持不变，渲染序不受影响。
	std::vector<uint32_t> m_prevSliceVboDemand;   ///< 上一并行帧各 slot 顶点占用（溢出则放大估计）
	std::vector<uint32_t> m_prevSliceSsboDemand;  ///< 上一并行帧各 slot 矩阵占用（溢出则放大估计）
	std::vector<uint32_t> m_prevSliceInstDemand;  ///< 上一并行帧各 slot 实例占用（溢出则放大估计）

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
	 * @brief 把裁剪栈栈顶矩形写入 OpenGL（启用 GL_SCISSOR_TEST 并调用 glScissor）。
	 *        要求栈非空。负责 y 轴翻转（GL 用左下原点）和退化矩形处理。
	 */
	void ApplyTopClipRectToGL();

	/**
	 * @brief 求两个屏幕像素矩形的交集（左上原点）。无交集时返回 w/h 为 0 的退化矩形。
	 */
	static ClipRect IntersectClip(const ClipRect& a, const ClipRect& b);

	/**
	 * @brief 将纹理 ID 绑定到批处理纹理列表，返回纹理单元索引。
	 * @param textureID OpenGL 纹理 ID
	 * @return 纹理单元索引
	 */
	int BindTexture(uint32_t textureID);

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
	uint32_t GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,
		int fontSize, const glm::vec4& color,
		int& outWidth, int& outHeight);

	/**
	 * @brief 通过 TTF 渲染一段文字并上传为一张新的 Vulkan 纹理。
	 *        调用方负责在不再需要时释放返回纹理。
	 * @return 成功返回 true，并填充 out；失败返回 false。
	 */
	bool RenderTextToVulkanTexture(const std::string& text, const std::string& fontKey,
		int fontSize, const glm::vec4& color, CachedText& out);

	/**
	 * @brief 清空常驻文字纹理缓存（释放 GL 纹理）。析构时调用。
	 */
	void ClearPinnedTextCache();

	/**
	 * @brief 调整批处理 VBO 的容量。
	 * @param newCapacity 新容量（顶点个数）
	 */
	void ResizeBatchBuffer(size_t newCapacity);

	// ==================== Record 路径辅助函数 ====================
	// 这些函数把对应的 DrawXxx 调用录制到当前 worker 的 WorkerRecord 中，不调任何
	// GL。每个函数对应一个公开 DrawXxx，做的事情是公开版批处理路径里"BindTexture
	// + AddMatrix + AddVertices + CheckBatch"那一段在 worker 上的等价物（但把全局
	// 槽位分配延迟到回放时）。

	void RecordDrawTexture(WorkerRecord& r,
		const Texture* tex, float x, float y, float width, float height,
		float rotation, const glm::vec4& tint);

	void RecordDrawTextureMatrix(WorkerRecord& r,
		const Texture* tex, const glm::mat4& transform,
		float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode);

	void RecordDrawTextureRegion(WorkerRecord& r,
		const Texture* tex,
		float srcX, float srcY, float srcW, float srcH,
		float dstX, float dstY, float dstW, float dstH,
		float rotation, const glm::vec4& tint);

	void RecordDrawCachedText(WorkerRecord& r,
		const CachedText& handle, float x, float y, float scale);

	void RecordDrawText(WorkerRecord& r,
		const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale);

	void RecordFillRect(WorkerRecord& r,
		float x, float y, float width, float height, const glm::vec4& color);

	void RecordDrawRect(WorkerRecord& r,
		float x, float y, float width, float height, const glm::vec4& color);

	void RecordDrawLine(WorkerRecord& r,
		float x1, float y1, float x2, float y2, const glm::vec4& color);

	void RecordDrawCircle(WorkerRecord& r,
		float cx, float cy, float radius, const glm::vec4& color, int segments);

	void RecordFillCircle(WorkerRecord& r,
		float cx, float cy, float radius, const glm::vec4& color, int segments);

	void RecordPushClipRect(WorkerRecord& r, int x, int y, int w, int h);
	void RecordPopClipRect(WorkerRecord& r);
	void RecordSetBlendMode(WorkerRecord& r, BlendMode mode);

	/**
	 * @brief Flush m_batchInstances to fr.instBuf, emit one vkCmdDraw using current m_currentBlendMode.
	 *        Called on BlendMode change, chunk overflow, end of frame, or before clip changes.
	 */
	void FlushInstances();
};

#endif