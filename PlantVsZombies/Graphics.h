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
#include <set>

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

inline constexpr float DRAW_CLIP_BOTTOM_DISABLED = 1.0e30f;  ///< 水平 shader 裁剪关闭值（逻辑像素 Y）

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
	float clipBottomY = DRAW_CLIP_BOTTOM_DISABLED;  // 逻辑坐标中保留 y <= 此值；大值表示不裁剪
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
 *        Stride 52 B under std430 scalar layout.
 */
struct InstanceRecord {
	float tA, tB, tC, tD;   // 16 B — pre-multiplied 2x2: cols (tA,tB) and (tC,tD)
	float tx, ty;           // 8 B  — world translation (already absorbs baseX + transform.x*Scale + offsets)
	float u0, v0;           // 8 B  — atlas UV top-left
	float u1, v1;           // 8 B  — atlas UV bottom-right
	uint32_t texSlot;       // 4 B  — bindless texture index
	uint32_t colorRGBA8;    // 4 B  — packed RGBA8 (r=lsb, a=msb), pre-tinted
	float clipBottomY = DRAW_CLIP_BOTTOM_DISABLED;  // 4 B — 逐实例水平裁剪线，避免切断实例批次
};
static_assert(sizeof(InstanceRecord) == 52, "InstanceRecord must be 52 bytes");

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
	// 光栅化时的超采样倍数（= physSize / fontSize）。全屏 letterbox 放大下，文字按物理
	// 像素光栅化以保持锐利；绘制时用它把 width/height 除回逻辑尺寸，保证屏幕布局不变。
	float superSample = 1.0f;
	// pinned 缓存代际号：AcquireTextTexture 时打上当时的 m_textGeneration。letterbox 变化
	// 清 pinned 缓存会递增代际号，使所有持有旧副本的消费者（如 CardDisplayComponent）能
	// 用 IsCachedTextStale 察觉自己手里的句柄已失效，避免读已销毁的 bindless 槽位。
	uint32_t generation = 0;
};

/**
 * @brief 图集内单个字形的 UV + 度量（物理像素口径）。
 */
struct GlyphInfo {
	float u0 = 0, v0 = 0, u1 = 0, v1 = 0;  // 图集内归一化 UV（左上 u0,v0 / 右下 u1,v1）
	int   pxW = 0, pxH = 0;                // 字形位图物理像素尺寸
	int   advance = 0;                     // 笔触前进（物理像素）
	int   bearingX = 0;                    // minx（物理像素）
	int   bearingY = 0;                    // maxy 相对基线（物理像素）
};

/**
 * @brief 一张 HUD 字形图集：单行打包的白色字形纹理（一个 bindless 槽），按 (字体,字号) 缓存。
 *        白色烘焙 + 顶点色 tint → 一张图集服务所有颜色。physSize 变化（letterbox）时整张重建。
 */
struct GlyphAtlas {
	pvz::VulkanTexture* vkTex = nullptr;     // 图集纹理句柄（ClearGlyphAtlases 负责还给 pool）
	uint32_t            textureID = 0;       // vkTex->bindlessIndex；0 = 未建/建失败
	int                 texW = 0, texH = 0;
	int                 ascent = 0;          // physSize 字体的 TTF_FontAscent（物理像素）
	int                 physSize = 0;        // 烘焙物理字号（= ComputeTextRasterSize 结果）
	float               superSample = 1.0f;  // physSize / fontSize
	std::unordered_map<uint32_t, GlyphInfo> glyphs;  // codepoint → 字形
	std::set<uint32_t>  covered;             // 已纳入的码点全集（重建时的烘焙集合）
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
	DeferredText,   ///< DrawText 延迟到主线程执行（payloadIdx 指向 textCmds）
	DeferredGlyphRun ///< DrawGlyphRun 延迟到主线程就地发射（payloadIdx 指向 glyphRunCmds）
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
	bool        onTop = false;  ///< true=回放末尾统一画(绝对顶层); false=就地交错画(对象同 z-order)
};

/**
 * @brief 延迟执行的 DrawGlyphRun 参数。worker 只记此命令、不碰图集/纹理；主线程 replay 时
 *        就地调 DrawGlyphRun 发射字形 quad（与对象同 z-order）。血量只需当前层，无 onTop 变体。
 */
struct DeferredGlyphRunCmd {
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
	BatchVertex* vboPtr = nullptr;  ///< 切片首个 BatchVertex 的映射指针
	glm::mat4* ssboPtr = nullptr;  ///< 切片首个 mat4 的映射指针
	InstanceRecord* instPtr = nullptr;  ///< 切片首个 InstanceRecord 的映射指针（Task 3）
	uint32_t        vboBaseVert = 0;        ///< vboPtr[0] 在整帧 VBO 中的绝对顶点下标
	uint32_t        ssboBaseMat = 0;        ///< ssboPtr[0] 在整帧 SSBO 中的绝对 mat4 下标
	uint32_t        instBaseIdx = 0;        ///< instPtr[0] 在整帧 instBuf 中的绝对下标（Task 3）
	uint32_t        vboCap = 0;        ///< 切片容量（BatchVertex 个数）
	uint32_t        ssboCap = 0;        ///< 切片容量（mat4 个数）
	uint32_t        instCap = 0;        ///< 切片容量（InstanceRecord 个数）（Task 3）
	uint32_t        vboCount = 0;        ///< 当前已写顶点数
	uint32_t        ssboCount = 0;        ///< 当前已写矩阵数
	uint32_t        instCount = 0;        ///< 当前已写 InstanceRecord 数（Task 3）
	uint32_t        vboDemand = 0;       ///< 本帧"想写"的顶点总数（含被容量拒绝的），供精确扩容 + 负载均衡权重
	uint32_t        ssboDemand = 0;      ///< 本帧"想写"的矩阵总数（含被拒绝的）
	uint32_t        instDemand = 0;      ///< 本帧"想写"的 InstanceRecord 总数（含被拒绝的）
	bool            vboOverflowed = false;   ///< vbo 写超容量（回放只画前 vboCount 个）
	bool            ssboOverflowed = false;  ///< ssbo 写超容量
	bool            instOverflowed = false;  ///< inst 写超容量
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
	std::vector<DeferredGlyphRunCmd> glyphRunCmds;  ///< DeferredGlyphRun 的 payload

	// 初始状态快照（BeginParallelRecord 时由主线程填充，SetWorkerSlot 时给 worker 用）
	glm::mat4              initialTopTransform = glm::mat4(1.0f);
	bool                   initialTopIsIdentity = true;
	std::vector<ClipRect>  initialClipStack;
	std::vector<float>     initialClipBottomStack;
	BlendMode              initialBlend = BlendMode::None;

	// 清空所有命令，但保留 vector capacity，下帧复用避免 realloc。
	// slice 不在这里重置——由 BeginParallelRecord 每帧重新切分并写入。
	void Reset() {
		cmds.clear();
		clips.clear();
		blendModes.clear();
		textCmds.clear();
		glyphRunCmds.clear();
		initialClipStack.clear();
		initialClipBottomStack.clear();
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
	std::vector<float>     clipBottomStack;
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

	/// 上一完整帧实际录入 command buffer 的 vkCmdDraw 次数，供性能回归测试使用。
	uint32_t GetLastFrameDrawCallCount() const { return m_lastFrameDrawCallCount; }

	/// 上一完整帧由 ClipRect 栈触发的 vkCmdSetScissor 次数，不含帧首默认 scissor。
	uint32_t GetLastFrameScissorChangeCount() const { return m_lastFrameScissorChangeCount; }

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
	 * @brief 压入一条水平裁剪线，只保留逻辑坐标 y <= bottomY 的像素。
	 *        裁剪值随 BatchVertex / InstanceRecord 送入 shader，不刷新批处理，也不生成
	 *        vkCmdSetScissor；适合在大量独立对象（如水中僵尸）周围细粒度使用。
	 *        嵌套时取更小的 bottomY。
	 */
	void PushClipBottom(float bottomY);

	/**
	 * @brief 弹出水平 shader 裁剪线；不会刷新批处理。
	 */
	void PopClipBottom();

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
	 * @brief 绘制文字（**当前层**：与调用处的绘制顺序同 z-order）。
	 *        在对象 Draw 循环内调用时，文字落在本对象之上、后续对象之下——参与正常 z-order。
	 *        若要文字盖住一切（HUD/调试 overlay），改用 DrawTextOnTop。
	 * @note  color 是 **0..255** 范围（ToSDLColor 直接 static_cast，不乘 255），勿写成 0..1 否则全透明。
	 * @param text     文字内容
	 * @param fontKey  字体键名
	 * @param fontSize 字体大小
	 * @param color    文字颜色（0..255）
	 * @param x        目标左上角 X 坐标
	 * @param y        目标左上角 Y 坐标
	 * @param scale    文字缩放系数（默认为1）
	 */
	void DrawText(const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale = 1.0f);

	/**
	 * @brief 绘制 **绝对顶层** 文字：盖在所有游戏对象之上（HUD / 调试 overlay 用）。
	 *        与 DrawText 的区别只在 z-order——本函数让文字凌驾于本帧后续所有几何之上。
	 *        worker 路径：标记 onTop，ReplayAndEndParallel 在所有 slot 几何 emit 完后统一渲染；
	 *        主线程串行：先 flush 已排队 batch/instance 再画（串行无回放阶段，近似顶层）。
	 *        若只想让文字与对象同层（如血量），用 DrawText。
	 * @note  color 是 0..255 范围，同 DrawText。
	 */
	void DrawTextOnTop(const std::string& text, const std::string& fontKey, int fontSize,
		const glm::vec4& color, float x, float y, float scale = 1.0f);

	/**
	 * @brief 用 HUD 字形图集逐字形绘制一段文字（与对象同 z-order，等价 DrawText 的层序）。
	 *        替代频繁变化的数字串（血量等）走的"整串→纹理"LRU 路径：字形只光栅化一次进图集，
	 *        变化的数字成本≈0。worker 路径记轻量命令、零光栅化；图集只在主线程构建。
	 * @note  color 是 **0..255** 范围，同 DrawText。建图集失败 / 任一缺字形时自动 fallback 到 DrawText。
	 * @warning 仅用于 **HUD 短串**（血量等小而固定的字符集）。图集是单行横排、covered 码点集只增不减：
	 *          拿它画可变长文本（整句中文）会让 covered 膨胀、图集宽度超过最大纹理边长 → 建图集永久
	 *          失败并退回 DrawText。任意 / 长文本请直接用 DrawText 或 DrawTextOnTop。
	 */
	void DrawGlyphRun(const std::string& text, const std::string& fontKey, int fontSize,
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
	 * @brief 判断一份 pinned 文字句柄是否已失效（底层纹理已被 ClearPinnedTextCache 销毁）。
	 *        消费者持有句柄副本时，应在主线程重绘前用它判断是否需要重新 AcquireTextTexture。
	 *        全屏切换会清 pinned 缓存并递增代际号，使旧句柄在此返回 true。
	 */
	bool IsCachedTextStale(const CachedText& handle) const {
		return handle.generation != m_textGeneration;
	}

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
	 * @brief 世界坐标点是否落在当前相机可见范围内（含像素 margin）。
	 *        内部用真实的 m_projection * m_viewMatrix 判定，故相机 zoom/pan/rotate 变化时自动正确。
	 *        用于视口剔除：屏幕外对象可跳过 DrawText 等批处理绘制（省 VBO + CPU）。
	 * @param marginPx 屏幕空间外扩边距（像素），避免边缘对象的文字/精灵提前消失。
	 */
	bool IsWorldPointVisible(float worldX, float worldY, float marginPx = 128.0f) const;

	/**
	 * @brief 重置摄像机到默认状态（位置归零、缩放1、旋转0）。
	 */
	void ResetCamera();

	/**
	 * @brief 逻辑坐标 → 世界坐标（应用摄像机位移、缩放、旋转的逆变换）。
	 *
	 * 坐标三层：帧缓冲像素 --ScreenToLogical--> 逻辑坐标 --LogicalToWorld--> 世界坐标。
	 * 本函数只负责后一段（相机变换）。入参是逻辑坐标(0..1100/0..600)，**不是**帧缓冲像素——
	 * 鼠标的 letterbox 逆变换已在 InputHandler::ProcessEvent 入口完成，多数调用点传的是 UI 布局坐标。
	 * @param logicalX 逻辑 X 坐标（UI 坐标空间）
	 * @param logicalY 逻辑 Y 坐标（UI 坐标空间）
	 * @return 对应的世界坐标
	 */
	glm::vec2 LogicalToWorld(float logicalX, float logicalY) const;

	/**
	 * @brief 世界坐标 → 逻辑坐标（LogicalToWorld 的反函数）。
	 * @param worldX 世界 X 坐标
	 * @param worldY 世界 Y 坐标
	 * @return 对应的逻辑坐标（UI 坐标空间，非帧缓冲像素）
	 */
	glm::vec2 WorldToLogical(float worldX, float worldY) const;

	// ==================== Letterbox（等比 + 黑边）全屏支持 ====================
	//
	// 逻辑分辨率恒为 1100×600（m_windowWidth/Height）。全屏时真实交换链尺寸变大，
	// 这里算出统一缩放比 + 居中黑边偏移，供三处共用：
	//   1) VulkanRenderer 默认 viewport（画面等比居中，黑边区域不画）
	//   2) ApplyTopClipRectToGL 的 scissor（逻辑裁剪框 → 帧缓冲像素）
	//   3) InputHandler 鼠标坐标（帧缓冲像素 → 逻辑坐标）
	// 窗口模式（真实尺寸==逻辑尺寸）时 scale=1、offset=0，退化为原行为。

	/**
	 * @brief 交换链重建后调用一次：读真实交换链尺寸，重算缩放比与黑边偏移。
	 *        m_vk 未初始化时退化为 scale=1、offset=0。
	 */
	void RecomputeLetterbox();

	/// 逻辑→帧缓冲 的统一缩放比（min(realW/logicalW, realH/logicalH)）。
	float GetLetterboxScale() const { return m_letterboxScale; }
	/// 居中黑边在帧缓冲中的左上偏移（像素）。
	glm::vec2 GetLetterboxOffset() const { return { m_letterboxOffsetX, m_letterboxOffsetY }; }
	/// 画面在帧缓冲中实际占用的矩形尺寸（logical * scale，像素）。
	glm::vec2 GetLetterboxViewportSize() const {
		return { m_windowWidth * m_letterboxScale, m_windowHeight * m_letterboxScale };
	}

	/// 把逻辑坐标裁剪框换算为帧缓冲像素裁剪框（scissor 用，不经过 viewport 变换）。
	void LogicalRectToFramebuffer(int lx, int ly, int lw, int lh,
		int32_t& outX, int32_t& outY, uint32_t& outW, uint32_t& outH) const;

	/// 帧缓冲像素 → 逻辑像素（仅 letterbox 逆变换，不含相机）。UI 鼠标坐标用。
	glm::vec2 ScreenToLogical(float screenX, float screenY) const {
		return { (screenX - m_letterboxOffsetX) / m_letterboxScale,
				 (screenY - m_letterboxOffsetY) / m_letterboxScale };
	}

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
	BlendMode m_currentBlendMode = BlendMode::None;   ///< 当前混合模式（被 SetBlendMode 公共 API 改，
	                                                  ///< 被 DrawTexture/DrawTextureMatrix 等 read 用作 per-vert bm 默认值；
	                                                  ///< AppendReanimInstance **不再修改它**，避免污染 shadow 等下游消费者）。
	BlendMode m_queuedInstanceBlend = BlendMode::Alpha;  ///< 主线程 instance 队列当前堆的 blend（独立于 m_currentBlendMode）。
	                                                     ///< FlushInstances 按它选 pipeline；AppendReanimInstance 用它判断是否需要 flush 切换。

	glm::mat4 m_projection = glm::mat4(1.0f);   ///< 正交投影矩阵
	glm::mat4 m_viewMatrix = glm::mat4(1.0f);   ///< 摄像机视图矩阵
	int m_windowWidth = 0;                      ///< 逻辑宽度（=SCENE_WIDTH，UI 坐标空间，不随全屏变化）
	int m_windowHeight = 0;                      ///< 逻辑高度（=SCENE_HEIGHT）

	// Letterbox 参数：由 RecomputeLetterbox() 从真实交换链尺寸算出。窗口模式下 scale=1、offset=0。
	float m_letterboxScale = 1.0f;              ///< 逻辑→帧缓冲 统一缩放比
	float m_letterboxOffsetX = 0.0f;            ///< 黑边左偏移（帧缓冲像素）
	float m_letterboxOffsetY = 0.0f;            ///< 黑边上偏移（帧缓冲像素）

	// 摄像机状态
	glm::vec2 m_cameraPos = glm::vec2(0.0f);  ///< 摄像机世界坐标
	float     m_cameraZoom = 1.0f;              ///< 缩放倍率
	float     m_cameraRotation = 0.0f;              ///< 旋转角度（度）

	std::vector<glm::mat4> m_transformStack;   ///< 变换矩阵栈
	std::vector<char>      m_transformIsIdentity; ///< 与 m_transformStack 平行：该层是否为单位阵（用于跳过冗余 mat4 乘法）

	std::vector<ClipRect> m_clipStack;          ///< 裁剪矩形栈（屏幕像素，已经过嵌套交集）
	std::vector<float> m_clipBottomStack;       ///< 不打断批处理的水平 shader 裁剪栈（逻辑像素 Y）

	// 批处理数据缓冲区
	std::vector<BatchVertex> m_batchVertices;   ///< 批处理顶点列表
	std::vector<glm::mat4> m_batchMatrices;     ///< 当前批次使用的变换矩阵列表
	std::vector<InstanceRecord> m_batchInstances;   ///< 主线程串行 instance 缓冲（worker 走 slice 不经此处）
	int m_batchInstancesLimit = 32768;              ///< 单次 flush 上限，~1.7 MB 一次 vkCmdDraw（仅切分 draw 段数，不影响总字节；逐帧 inst 缓冲 grow-on-demand，见 Graphics.cpp）
	bool m_useInstancePath = true;   ///< Task 7: false强制走 slow path 做 A/B baseline

	size_t m_batchBufferCapacity = 0;             ///< 当前 VBO 容量（顶点个数）

	bool m_batchMode = true;                      ///< 是否启用批处理模式
	uint32_t m_frameDrawCallCount = 0;             ///< 当前帧实际 vkCmdDraw 次数
	uint32_t m_lastFrameDrawCallCount = 0;         ///< 上一完整帧实际 vkCmdDraw 次数
	uint32_t m_frameScissorChangeCount = 0;        ///< 当前帧由 ClipRect 栈触发的 scissor 变更次数
	uint32_t m_lastFrameScissorChangeCount = 0;    ///< 上一完整帧由 ClipRect 栈触发的 scissor 变更次数

	uint32_t m_whiteTexture = 0;   ///< 1×1 纯白纹理 bindless 槽位，用于 FillRect 批处理

	// Phase 3a：清屏颜色（归一化 [0,1]），SetClearColor 写入、GetClearColor 读出，供 VulkanRenderer 使用。
	float m_clearR = 1.0f, m_clearG = 1.0f, m_clearB = 1.0f, m_clearA = 1.0f;

	// Phase 3b：Vulkan 真渲染的 PIMPL 状态（pipeline / 逐帧 VBO+SSBO / descriptor set 等），
	// 定义在 Graphics.cpp 内部，避免把 vulkan.h 暴露给整个工程。
	struct VulkanGraphicsState;
	std::unique_ptr<VulkanGraphicsState> m_vk;

	static const int TEXT_CACHE_MAX_SIZE = 1024;  ///< 文字缓存最大条目数（LRU 淘汰）
	std::list<std::string> m_textCacheOrder;     ///< LRU 顺序链表（front = 最近使用）
	std::unordered_map<std::string,
		std::pair<CachedText, std::list<std::string>::iterator>> m_textCache;  ///< 文字纹理 LRU 缓存
	std::unordered_map<std::string, CachedText> m_pinnedTextCache;  ///< 常驻文字纹理缓存（AcquireTextTexture 使用，不淘汰）
	uint32_t m_textGeneration = 0;  ///< pinned 缓存代际号；ClearPinnedTextCache 递增，用于失效持有方的旧句柄
	std::unordered_map<std::string, GlyphAtlas> m_glyphAtlases;  ///< HUD 字形图集，键 = "fontKey|fontSize"

	// ==================== 多线程录制状态 ====================
	std::vector<WorkerRecord>      m_workerRecords;       ///< 每个 worker slot 一份的录制缓冲
	std::vector<WorkerThreadState> m_workerStates;        ///< 每个 worker slot 一份的栈存储
	int                            m_numActiveWorkers = 0;///< 本轮 Dispatch 的有效 slot 数（=Replay 时遍历的上限）
	bool                           m_parallelDrawEnabled = true; ///< 是否启用并行 record/replay 路径

	// Phase 5：BeginParallelRecord 切片时记录主线程基准游标和切片总占用，
	// ReplayAndEndParallel 收尾时据此跳过整段并行区域。
	// 用 uint64_t 而非 VkDeviceSize 是为了避免在 Graphics.h 里引入 vulkan.h（保持 PIMPL）。
	uint64_t m_parallelBaseVbo = 0;
	uint64_t m_parallelBaseSsbo = 0;
	uint64_t m_parallelBaseInst = 0;
	uint64_t m_parallelVboBytes = 0;
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
	 * @brief 返回当前线程生效的水平 shader 裁剪线；无裁剪时返回关闭值。
	 */
	float CurrentClipBottom() const;

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
		int& outWidth, int& outHeight, float& outSuperSample);

	/**
	 * @brief 通过 TTF 渲染一段文字并上传为一张新的 Vulkan 纹理。
	 *        调用方负责在不再需要时释放返回纹理。
	 * @return 成功返回 true，并填充 out；失败返回 false。
	 */
	bool RenderTextToVulkanTexture(const std::string& text, const std::string& fontKey,
		int fontSize, const glm::vec4& color, CachedText& out);

	/**
	 * @brief 取/建 (fontKey,fontSize) 的字形图集，并确保 needed 里的码点全部已烘入（当帧收敛）。
	 *        physSize 变化（letterbox）或有新码点时整张重建。返回引用恒有效（unordered_map 引用稳定）。
	 */
	GlyphAtlas& GetOrBuildGlyphAtlas(const std::string& fontKey, int fontSize,
		const std::vector<uint32_t>& needed);

	/**
	 * @brief 按 atlas.covered 全集（重新）烘焙图集：逐字形 TTF 度量+渲染→单行打包→上传一张纹理。
	 *        旧纹理先还给 pool。失败时令 atlas.textureID=0（上层 fallback）。
	 */
	void BuildGlyphAtlas(const std::string& fontKey, int fontSize, GlyphAtlas& atlas);

	/**
	 * @brief 释放全部字形图集纹理（还给 pool），清空缓存。ShutdownVulkan / ClearTextCache 时调。
	 */
	void ClearGlyphAtlases();

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
		const glm::vec4& color, float x, float y, float scale, bool onTop);

	void RecordDrawGlyphRun(WorkerRecord& r,
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
