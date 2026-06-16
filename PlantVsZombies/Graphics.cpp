#include "Graphics.h"
#include "Logger.h"
#include "Profiler.h"

#include "./Renderer/VulkanContext.h"
#include "./Renderer/VulkanRenderer.h"
#include "./Renderer/VulkanTexturePool.h"
#include "./Renderer/VulkanBuffer.h"
#include "./Renderer/VulkanPipeline.h"

#include <cstring>
#include <array>
#include <algorithm>
#include <cmath>

namespace {
	// Phase 3b — BatchVertex 顶点输入描述（与 Graphics.h struct BatchVertex 对齐）
	// stride=44B：vec2 pos(8) + vec2 uv(8) + uvec2 indices(8) + vec4 color(16) + float blendMode(4)
	// blendMode 字段仅 CPU 侧分段使用，shader 不读，故不暴露为 attribute。
	constexpr VkVertexInputBindingDescription kBatchVertexBinding = {
		/*binding*/0, /*stride*/sizeof(BatchVertex), VK_VERTEX_INPUT_RATE_VERTEX
	};
	constexpr VkVertexInputAttributeDescription kBatchVertexAttrs[4] = {
		{ /*location*/0, /*binding*/0, VK_FORMAT_R32G32_SFLOAT,        offsetof(BatchVertex, x)         },
		{ /*location*/1, /*binding*/0, VK_FORMAT_R32G32_SFLOAT,        offsetof(BatchVertex, u)         },
		{ /*location*/2, /*binding*/0, VK_FORMAT_R32G32_UINT,          offsetof(BatchVertex, texIndex)  },
		{ /*location*/3, /*binding*/0, VK_FORMAT_R32G32B32A32_SFLOAT,  offsetof(BatchVertex, r)         },
	};

	// 每帧持久映射 host-visible 缓冲容量。
	// VBO: 128 MB ≈ 3M BatchVertex ≈ 254*2k quad。实测有些 scene 在加载/瞬时会摸到顶点，
	// 给 5× 余量避免 drop。两帧 in flight 共 128 MB host-visible VRAM
	// SSBO: 32 MB / 64B = 262144*2 mat4，跟 VBO 上限对得上（典型 6 顶点/矩阵比例）
	constexpr VkDeviceSize VBO_BYTES_PER_FRAME = 128u * 1024u * 1024u;
	constexpr VkDeviceSize SSBO_BYTES_PER_FRAME = 32u * 1024u * 1024u;
	// InstanceRecord: 48 B/instance. 64 MB / 48 B ≈ 1.4M 容量。
	// 11000 zombie × ~15 track × 3 (normal+glow+overlay) ≈ 500k；32MB(≈700k) 在
	// "对象很多很多"场景下被 worker slice 瓜分后单 slot 溢出（[Graphics] Worker slot overflow），
	// 实测确认 inst 是瓶颈，翻倍到 64MB。代价 +32MB×2 帧 = +64MB host-visible VRAM。
	constexpr VkDeviceSize INST_BYTES_PER_FRAME = 64u * 1024u * 1024u;
} // anonymous

// Phase 3b — Vulkan 端持有的全部渲染资源。PIMPL 在 Graphics.cpp 内部定义，
// 避免把 vulkan.h 暴露给整个工程。
struct Graphics::VulkanGraphicsState {
	pvz::VulkanContext* ctx = nullptr;
	pvz::VulkanRenderer* renderer = nullptr;
	pvz::VulkanTexturePool* texPool = nullptr;

	// set=0 描述：matrix SSBO（binding=0），逐帧一个 set 绑定到自家 SSBO
	VkDescriptorSetLayout matrixSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool      matrixPool = VK_NULL_HANDLE;

	struct PerFrame {
		std::unique_ptr<pvz::VulkanBuffer> vbo;          // host-visible 持久映射
		std::unique_ptr<pvz::VulkanBuffer> ssbo;         // host-visible 持久映射
		std::unique_ptr<pvz::VulkanBuffer> instBuf;      // host-visible 持久映射（Task 3）
		VkDescriptorSet                    matrixSet = VK_NULL_HANDLE;
		VkDescriptorSet                    instSet = VK_NULL_HANDLE;  // Task 3
		VkDeviceSize                       vboCursor = 0;   // 当前写入字节偏移
		VkDeviceSize                       ssboCursor = 0;
		VkDeviceSize                       instCursor = 0;   // Task 3
		bool                               overflowWarned = false;  // 本帧已打过 overflow 警告？
	};
	static constexpr uint32_t FRAMES = pvz::VulkanRenderer::FRAMES_IN_FLIGHT;
	std::array<PerFrame, FRAMES> frames;
	uint32_t frameIdx = 0;  // 与 VulkanRenderer 帧索引同步

	std::unique_ptr<pvz::VulkanPipeline> pipeBatchAlpha;
	std::unique_ptr<pvz::VulkanPipeline> pipeBatchAdd;

	// Phase 4 — instance pipelines for reanim sprites (consumed by Task 5).
	// Independent set=0 descriptor layout: binding=0 = InstanceRecord SSBO.
	// per-frame instance buffer + descriptor set are added in Task 3.
	VkDescriptorSetLayout                 instanceSetLayout = VK_NULL_HANDLE;
	VkDescriptorPool                      instancePool = VK_NULL_HANDLE;
	std::unique_ptr<pvz::VulkanPipeline>  pipeInstAlpha;
	std::unique_ptr<pvz::VulkanPipeline>  pipeInstAdd;

	// 1x1 白色纹理：FillRect / DrawText 等"无纹理但走 batch 路径"的画法用它的 bindless 槽。
	pvz::VulkanTexture* whiteTex = nullptr;

	bool frameOpen = false;
};

// 与 glm::mat4(1.0f) 精确比较：仅用于快路判定，假阴性只会多做一次乘法（结果仍正确），不会出错。
static inline bool IsIdentityMat(const glm::mat4& m) {
	return m == glm::mat4(1.0f);
}

// ==================== 多线程录制状态（thread_local） ====================
//
// 这些指针在主线程上始终为 nullptr，所以所有公开 DrawXxx 路径在主线程上保持原行为
// 不变；只有 SetWorkerSlot() 在 worker 线程把它们置位后，DrawXxx 才走 Record 路径。
// 注意：thread_local 是按线程而非按 slot 的。Dispatch 每轮把同一个 worker 线程绑到
// 同一个 slot 上跑完一整段（slot = idx），所以指针在该线程内稳定。
namespace {
	thread_local WorkerRecord* tl_record = nullptr;
	thread_local std::vector<glm::mat4>* tl_transformStack = nullptr;
	thread_local std::vector<char>* tl_transformIsIdentity = nullptr;
	thread_local std::vector<ClipRect>* tl_clipStack = nullptr;
	thread_local BlendMode               tl_blend = BlendMode::None;

	// Phase 5：worker 直接把绝对 bindless 槽位写进 BatchVertex.texIndex、把
	// (slice.ssboBaseMat + slice.ssboCount) 写进 BatchVertex.matrixIndex，replay 不再
	// 做任何索引重映射，所以 InternTex / InternMat 已删除。

	// Phase 5：从 FlushBatch（CPU 顶点源）和 ReplayAndEndParallel（每个 worker slot 的
	// mapped VBO 切片）共用的"分段 + vkCmdDraw"。
	// 前置条件：调用方已经做过 vkCmdBindVertexBuffers，cb 处于 record 状态。
	// 参数：
	//   firstVert : 这段顶点在已绑定 VBO 中的绝对起始下标（vkCmdDraw 的 firstVertex 用它）
	//   vertCount : 要 emit 的顶点数（按 6 顶点对齐，否则尾部余数被裁掉）
	//   scan      : scan[0..vertCount) 应当反映 [firstVert..firstVert+vertCount) 的 blendMode 字段。
	//               FlushBatch 路径下 scan = m_batchVertices.data()（CPU 顺序内存，cache 友好）；
	//               Replay 路径下 scan = (BatchVertex*)mapped + firstVert（host-coherent map，按顺序读）。
	void EmitDrawRange(VkCommandBuffer cb,
		uint32_t firstVert, uint32_t vertCount,
		const BatchVertex* scan,
		pvz::VulkanPipeline* pipeAlpha, pvz::VulkanPipeline* pipeAdd,
		const VkDescriptorSet sets[2],
		const glm::mat4& projView)
	{
		if (vertCount == 0 || !scan) return;
		// 严格按 6 顶点 stride 分段；尾部余数（不可能正常出现）丢弃，避免越界。
		const uint32_t aligned = (vertCount / 6) * 6;
		if (aligned == 0) return;

		auto emit = [&](uint32_t segStart, uint32_t segEnd, float bm) {
			if (segEnd <= segStart) return;
			pvz::VulkanPipeline* pipe = (bm >= 0.5f) ? pipeAdd : pipeAlpha;
			vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
			vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Layout(),
				0, 2, sets, 0, nullptr);
			vkCmdPushConstants(cb, pipe->Layout(), VK_SHADER_STAGE_VERTEX_BIT,
				0, sizeof(glm::mat4), &projView);
			vkCmdDraw(cb, segEnd - segStart, 1, firstVert + segStart, 0);
			};

		uint32_t segStart = 0;
		float curBm = scan[0].blendMode;
		for (uint32_t i = 6; i < aligned; i += 6) {
			const float bm = scan[i].blendMode;
			if (bm != curBm) {
				emit(segStart, i, curBm);
				segStart = i;
				curBm = bm;
			}
		}
		emit(segStart, aligned, curBm);
	}
}

Graphics::Graphics() {
	m_transformStack.push_back(glm::mat4(1.0f));
	m_transformIsIdentity.push_back(1);
	this->SetBlendMode(BlendMode::Alpha);
}

Graphics::~Graphics() {
	// Phase 3c：顺序很重要——文字缓存里的纹理需要 pool 还活着才能正确归还 bindless 槽，
	// 所以先 Clear*TextCache，再 ShutdownVulkan 释放 pipeline / 缓冲 / matrix descriptor pool。
	// 如果 GameApp 已经先调过 ShutdownVulkan，Clear* 会走 m_vk==null 的 else 分支，由 pool::Shutdown 兜底回收。
	ClearTextCache();
	ClearPinnedTextCache();
	ShutdownVulkan();
	m_batchVertices.clear();
	m_batchMatrices.clear();
	m_batchTextures.clear();
}

bool Graphics::Initialize(int windowWidth, int windowHeight) {
	// Phase 5 cleanup：所有 GL 资源 / 批处理上限都已删除。Vulkan 容量在 InitializeVulkan 里建立。
	// 这里只剩窗口尺寸 + 投影矩阵的设置。
	SetWindowSize(windowWidth, windowHeight);
	return true;
}

void Graphics::SetWindowSize(int width, int height) {
	m_windowWidth = width;
	m_windowHeight = height;
	m_projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
	// Phase 3a：视口由 Vulkan dynamic state 处理，CPU 侧只更新投影矩阵。
}

bool Graphics::IsWorldPointVisible(float worldX, float worldY, float marginPx) const {
	// 用当前 projView 把世界点投到裁剪空间。ortho 下 w==1，无需透视除法，clip.xy 即 NDC。
	const glm::vec4 clip = (m_projection * m_viewMatrix) * glm::vec4(worldX, worldY, 0.0f, 1.0f);
	// 像素 margin → NDC：NDC 全宽 2 对应窗口像素全宽。zoom 已包含在 m_viewMatrix 里，margin 是屏幕空间量。
	const float mx = (m_windowWidth  > 0) ? (2.0f * marginPx / (float)m_windowWidth)  : 0.0f;
	const float my = (m_windowHeight > 0) ? (2.0f * marginPx / (float)m_windowHeight) : 0.0f;
	return clip.x >= -1.0f - mx && clip.x <= 1.0f + mx
		&& clip.y >= -1.0f - my && clip.y <= 1.0f + my;
}

// ==================== Phase 3b — Vulkan 接入 ====================

bool Graphics::InitializeVulkan(pvz::VulkanContext* ctx,
	pvz::VulkanRenderer* renderer,
	pvz::VulkanTexturePool* pool) {
	if (!ctx || !renderer || !pool) {
		LOG_ERROR("Graphics") << "InitializeVulkan 参数为空";
		return false;
	}
	if (m_vk) {
		LOG_WARN("Graphics") << "InitializeVulkan 已初始化";
		return false;
	}

	auto state = std::make_unique<VulkanGraphicsState>();
	state->ctx = ctx;
	state->renderer = renderer;
	state->texPool = pool;

	VkDevice dev = ctx->Device();

	// 1) set=0 layout：binding=0 SSBO（matrix），VERTEX 阶段
	{
		VkDescriptorSetLayoutBinding b{};
		b.binding = 0;
		b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		b.descriptorCount = 1;
		b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		lci.bindingCount = 1;
		lci.pBindings = &b;
		if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &state->matrixSetLayout) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "matrix SSBO descriptor set layout 创建失败";
			return false;
		}
	}

	// 1b) instance set=0 layout：binding=0 InstanceRecord SSBO，VERTEX 阶段
	{
		VkDescriptorSetLayoutBinding b{};
		b.binding = 0;
		b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		b.descriptorCount = 1;
		b.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

		VkDescriptorSetLayoutCreateInfo lci{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
		lci.bindingCount = 1;
		lci.pBindings = &b;
		if (vkCreateDescriptorSetLayout(dev, &lci, nullptr, &state->instanceSetLayout) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "instance SSBO descriptor set layout 创建失败";
			return false;
		}
	}

	// 2) descriptor pool：能装 FRAMES 个 set，每个 set 一个 SSBO
	{
		VkDescriptorPoolSize ps{};
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = VulkanGraphicsState::FRAMES;

		VkDescriptorPoolCreateInfo pci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pci.maxSets = VulkanGraphicsState::FRAMES;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;
		if (vkCreateDescriptorPool(dev, &pci, nullptr, &state->matrixPool) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "matrix descriptor pool 创建失败";
			return false;
		}
	}

	// 2b) instance descriptor pool：能装 FRAMES 个 instance set，每个 set 一个 SSBO
	{
		VkDescriptorPoolSize ps{};
		ps.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		ps.descriptorCount = VulkanGraphicsState::FRAMES;

		VkDescriptorPoolCreateInfo pci{ VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
		pci.maxSets = VulkanGraphicsState::FRAMES;
		pci.poolSizeCount = 1;
		pci.pPoolSizes = &ps;
		if (vkCreateDescriptorPool(dev, &pci, nullptr, &state->instancePool) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "instance descriptor pool 创建失败";
			return false;
		}
	}

	// 3) 逐帧资源：VBO + SSBO + descriptor set
	for (uint32_t i = 0; i < VulkanGraphicsState::FRAMES; ++i) {
		auto& fr = state->frames[i];

		fr.vbo = std::make_unique<pvz::VulkanBuffer>();
		fr.ssbo = std::make_unique<pvz::VulkanBuffer>();
		if (!fr.vbo->Create(ctx, VBO_BYTES_PER_FRAME,
			VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, /*hostVisible*/true)) return false;
		if (!fr.ssbo->Create(ctx, SSBO_BYTES_PER_FRAME,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, /*hostVisible*/true)) return false;
		fr.instBuf = std::make_unique<pvz::VulkanBuffer>();
		if (!fr.instBuf->Create(ctx, INST_BYTES_PER_FRAME,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, /*hostVisible*/true)) return false;

		VkDescriptorSetAllocateInfo ai{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		ai.descriptorPool = state->matrixPool;
		ai.descriptorSetCount = 1;
		ai.pSetLayouts = &state->matrixSetLayout;
		if (vkAllocateDescriptorSets(dev, &ai, &fr.matrixSet) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "matrix descriptor set 分配失败";
			return false;
		}

		VkDescriptorBufferInfo bi{};
		bi.buffer = fr.ssbo->Handle();
		bi.offset = 0;
		bi.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet w{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		w.dstSet = fr.matrixSet;
		w.dstBinding = 0;
		w.descriptorCount = 1;
		w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		w.pBufferInfo = &bi;
		vkUpdateDescriptorSets(dev, 1, &w, 0, nullptr);

		// Instance descriptor set: alloc from instancePool, bind binding=0 to this frame's instBuf
		VkDescriptorSetAllocateInfo aiInst{ VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
		aiInst.descriptorPool = state->instancePool;
		aiInst.descriptorSetCount = 1;
		aiInst.pSetLayouts = &state->instanceSetLayout;
		if (vkAllocateDescriptorSets(dev, &aiInst, &fr.instSet) != VK_SUCCESS) {
			LOG_ERROR("Graphics") << "instance descriptor set 分配失败";
			return false;
		}

		VkDescriptorBufferInfo biInst{};
		biInst.buffer = fr.instBuf->Handle();
		biInst.offset = 0;
		biInst.range = VK_WHOLE_SIZE;

		VkWriteDescriptorSet wInst{ VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };
		wInst.dstSet = fr.instSet;
		wInst.dstBinding = 0;
		wInst.descriptorCount = 1;
		wInst.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		wInst.pBufferInfo = &biInst;
		vkUpdateDescriptorSets(dev, 1, &wInst, 0, nullptr);
	}

	// 4) 两条 batch pipeline（alpha / additive），共享 layout 形状但各自单独 create
	{
		const VkDescriptorSetLayout sets[2] = {
			state->matrixSetLayout,        // set=0 matrix SSBO
			pool->DescriptorSetLayout(),   // set=1 bindless textures
		};

		pvz::VulkanPipeline::Desc desc{};
		desc.vertSpvPath = "Shader/spv/batch.vert.spv";
		desc.fragSpvPath = "Shader/spv/batch.frag.spv";
		desc.colorFormat = ctx->SwapchainFormat();
		desc.setLayouts = sets;
		desc.setLayoutCount = 2;
		desc.pushConstantSize = sizeof(glm::mat4);
		desc.pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;
		desc.vertexBindings = &kBatchVertexBinding;
		desc.vertexBindingCount = 1;
		desc.vertexAttributes = kBatchVertexAttrs;
		desc.vertexAttributeCount = (uint32_t)(sizeof(kBatchVertexAttrs) / sizeof(kBatchVertexAttrs[0]));

		desc.alphaBlend = true;
		desc.additiveBlend = false;
		state->pipeBatchAlpha = std::make_unique<pvz::VulkanPipeline>();
		if (!state->pipeBatchAlpha->Initialize(ctx, desc)) return false;

		desc.alphaBlend = false;
		desc.additiveBlend = true;
		state->pipeBatchAdd = std::make_unique<pvz::VulkanPipeline>();
		if (!state->pipeBatchAdd->Initialize(ctx, desc)) return false;
	}

	// 4b) 两条 instance pipeline（alpha / additive）。
	//     使用独立的 instanceSetLayout (set=0 binding=0 = InstanceRecord SSBO)；
	//     与 batch 共用 set=1 (bindless textures)。无 vertex input — gl_VertexIndex
	//     + gl_InstanceIndex 由 shader 自展开 6 顶点。Task 3 创建 per-frame instance
	//     buffer + descriptor set，Task 5 接 reanim 走这条路径。
	{
		const VkDescriptorSetLayout instSets[2] = {
			state->instanceSetLayout,      // set=0 instance SSBO
			pool->DescriptorSetLayout(),   // set=1 bindless textures
		};

		pvz::VulkanPipeline::Desc desc{};
		desc.vertSpvPath = "Shader/spv/reanim_inst.vert.spv";
		desc.fragSpvPath = "Shader/spv/reanim_inst.frag.spv";
		desc.colorFormat = ctx->SwapchainFormat();
		desc.setLayouts = instSets;
		desc.setLayoutCount = 2;
		desc.pushConstantSize = sizeof(glm::mat4);
		desc.pushConstantStages = VK_SHADER_STAGE_VERTEX_BIT;
		desc.vertexBindings = nullptr;
		desc.vertexBindingCount = 0;
		desc.vertexAttributes = nullptr;
		desc.vertexAttributeCount = 0;

		desc.alphaBlend = true;
		desc.additiveBlend = false;
		state->pipeInstAlpha = std::make_unique<pvz::VulkanPipeline>();
		if (!state->pipeInstAlpha->Initialize(ctx, desc)) {
			LOG_ERROR("Graphics") << "pipeInstAlpha 创建失败";
			return false;
		}

		desc.alphaBlend = false;
		desc.additiveBlend = true;
		state->pipeInstAdd = std::make_unique<pvz::VulkanPipeline>();
		if (!state->pipeInstAdd->Initialize(ctx, desc)) {
			LOG_ERROR("Graphics") << "pipeInstAdd 创建失败";
			return false;
		}
	}

	// 5) 1x1 白色纹理：FillRect / DrawText 等画法用它的 bindless 槽
	{
		const uint8_t white[4] = { 255, 255, 255, 255 };
		state->whiteTex = pool->CreateTextureRGBA8(1, 1, white);
		if (!state->whiteTex) {
			LOG_ERROR("Graphics") << "白色纹理上传失败";
			return false;
		}
		m_whiteTexture = state->whiteTex->bindlessIndex;
	}

	m_vk = std::move(state);
	LOG_INFO("Graphics") << "Vulkan 接入完成。VBO=" << (VBO_BYTES_PER_FRAME / 1024 / 1024)
		<< "MB/frame, SSBO=" << (SSBO_BYTES_PER_FRAME / 1024 / 1024)
		<< "MB/frame, white tex bindless=" << m_whiteTexture;
	return true;
}

void Graphics::ShutdownVulkan() {
	if (!m_vk) return;
	VkDevice dev = m_vk->ctx ? m_vk->ctx->Device() : VK_NULL_HANDLE;
	if (dev) vkDeviceWaitIdle(dev);

	// 白色纹理交还给 pool
	if (m_vk->whiteTex && m_vk->texPool) {
		m_vk->texPool->DestroyTexture(m_vk->whiteTex);
		m_vk->whiteTex = nullptr;
	}

	m_vk->pipeBatchAlpha.reset();
	m_vk->pipeBatchAdd.reset();
	m_vk->pipeInstAlpha.reset();
	m_vk->pipeInstAdd.reset();

	for (auto& fr : m_vk->frames) {
		fr.vbo.reset();
		fr.ssbo.reset();
		fr.matrixSet = VK_NULL_HANDLE;  // pool 销毁时一起回收
		fr.vboCursor = fr.ssboCursor = 0;
	}

	if (dev) {
		if (m_vk->matrixPool)      vkDestroyDescriptorPool(dev, m_vk->matrixPool, nullptr);
		if (m_vk->matrixSetLayout) vkDestroyDescriptorSetLayout(dev, m_vk->matrixSetLayout, nullptr);
		if (m_vk->instancePool)      vkDestroyDescriptorPool(dev, m_vk->instancePool, nullptr);
		if (m_vk->instanceSetLayout) vkDestroyDescriptorSetLayout(dev, m_vk->instanceSetLayout, nullptr);
	}
	m_vk->matrixPool = VK_NULL_HANDLE;
	m_vk->matrixSetLayout = VK_NULL_HANDLE;
	m_vk->instancePool = VK_NULL_HANDLE;
	m_vk->instanceSetLayout = VK_NULL_HANDLE;

	m_vk.reset();
}

bool Graphics::BeginFrame() {
	if (!m_vk || !m_vk->renderer) return false;
	if (m_vk->frameOpen) {
		LOG_WARN("Graphics") << "BeginFrame 上一帧未 EndFrame";
		return false;
	}

	if (!m_vk->renderer->BeginFrame(m_clearR, m_clearG, m_clearB, m_clearA)) {
		return false;
	}

	// 延迟纹理删除：此刻本帧 slot 的 fence 已被 renderer->BeginFrame 等过，回收"已过
	// FRAMES_IN_FLIGHT 帧"的待删纹理最安全。取代旧 DestroyTexture 里的 vkDeviceWaitIdle。
	if (m_vk->texPool) m_vk->texPool->BeginFrameTick();

	// Letterbox：renderer 已设铺满交换链的默认 viewport，这里覆盖成等比居中的矩形。
	// 视口变换把所有几何体约束进该矩形，黑边区域无顶点 → 保持清屏色（黑）。
	// 保留负高度 Y 翻转（沿用 GL 风格 top=0 正交矩阵）。窗口模式 scale=1/offset=0 时退化为原 viewport。
	{
		VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
		if (cb != VK_NULL_HANDLE) {
			float vpW = m_windowWidth * m_letterboxScale;
			float vpH = m_windowHeight * m_letterboxScale;
			VkViewport vp{};
			vp.x = m_letterboxOffsetX;
			vp.y = m_letterboxOffsetY + vpH;   // 底边，配合负高度翻转
			vp.width = vpW;
			vp.height = -vpH;
			vp.minDepth = 0.0f;
			vp.maxDepth = 1.0f;
			vkCmdSetViewport(cb, 0, 1, &vp);
		}
	}

	m_vk->frameIdx = m_vk->renderer->CurrentFrameIdx();
	auto& fr = m_vk->frames[m_vk->frameIdx];
	fr.vboCursor = 0;
	fr.ssboCursor = 0;
	fr.instCursor = 0;  // Task 3
	fr.overflowWarned = false;
	m_vk->frameOpen = true;
	return true;
}

bool Graphics::EndFrame() {
	if (!m_vk || !m_vk->frameOpen) return false;
	FlushBatch();
	FlushInstances();
	m_vk->frameOpen = false;
	return m_vk->renderer->EndFrame();
}

void Graphics::PushTransform(const glm::mat4& transform) {
	if (tl_record) {
		// worker：只动 thread-local 栈，不进 record 流（每次 DrawXxx 已把 finalMatrix 烘到 record 里）
		const bool curId = tl_transformIsIdentity->back() != 0;
		const bool tId = IsIdentityMat(transform);
		glm::mat4 newTop = curId ? transform : (tl_transformStack->back() * transform);
		tl_transformStack->push_back(newTop);
		tl_transformIsIdentity->push_back((curId && tId) ? 1 : 0);
		return;
	}
	const bool curId = m_transformIsIdentity.back() != 0;
	const bool tId = IsIdentityMat(transform);
	// 栈顶为单位阵时无需相乘，直接取 transform（这本身也消除一次冗余乘法）
	glm::mat4 newTop = curId ? transform : (m_transformStack.back() * transform);
	m_transformStack.push_back(newTop);
	m_transformIsIdentity.push_back((curId && tId) ? 1 : 0);
}

void Graphics::PopTransform() {
	if (tl_record) {
		if (tl_transformStack->size() > 1) {
			tl_transformStack->pop_back();
			tl_transformIsIdentity->pop_back();
		}
		else {
			LOG_WARN("Graphics") << "PopTransform (worker) failed: stack underflow.";
		}
		return;
	}
	if (m_transformStack.size() > 1) {
		m_transformStack.pop_back();
		m_transformIsIdentity.pop_back();
	}
	else {
		LOG_WARN("Graphics") << "PopTransform failed: stack underflow.";
	}
}

void Graphics::SetIdentity() {
	if (tl_record) {
		if (!tl_transformStack->empty()) {
			tl_transformStack->back() = glm::mat4(1.0f);
			tl_transformIsIdentity->back() = 1;
		}
		return;
	}
	if (!m_transformStack.empty()) {
		m_transformStack.back() = glm::mat4(1.0f);
		m_transformIsIdentity.back() = 1;
	}
}

void Graphics::Translate(float x, float y, float z) {
	if (tl_record) {
		if (tl_transformStack->empty()) return;
		tl_transformStack->back() = glm::translate(tl_transformStack->back(), glm::vec3(x, y, z));
		tl_transformIsIdentity->back() = 0;
		return;
	}
	if (m_transformStack.empty()) return;
	m_transformStack.back() = glm::translate(m_transformStack.back(), glm::vec3(x, y, z));
	m_transformIsIdentity.back() = 0;
}

void Graphics::Rotate(float angleDegrees, float x, float y, float z) {
	if (tl_record) {
		if (tl_transformStack->empty()) return;
		tl_transformStack->back() = glm::rotate(tl_transformStack->back(), glm::radians(angleDegrees), glm::vec3(x, y, z));
		tl_transformIsIdentity->back() = 0;
		return;
	}
	if (m_transformStack.empty()) return;
	m_transformStack.back() = glm::rotate(m_transformStack.back(), glm::radians(angleDegrees), glm::vec3(x, y, z));
	m_transformIsIdentity.back() = 0;
}

void Graphics::Scale(float sx, float sy, float sz) {
	if (tl_record) {
		if (tl_transformStack->empty()) return;
		tl_transformStack->back() = glm::scale(tl_transformStack->back(), glm::vec3(sx, sy, sz));
		tl_transformIsIdentity->back() = 0;
		return;
	}
	if (m_transformStack.empty()) return;
	m_transformStack.back() = glm::scale(m_transformStack.back(), glm::vec3(sx, sy, sz));
	m_transformIsIdentity.back() = 0;
}

// ==================== 裁剪栈（PushClipRect / PopClipRect） ====================

ClipRect Graphics::IntersectClip(const ClipRect& a, const ClipRect& b) {
	int x1 = std::max(a.x, b.x);
	int y1 = std::max(a.y, b.y);
	int x2 = std::min(a.x + a.w, b.x + b.w);
	int y2 = std::min(a.y + a.h, b.y + b.h);
	ClipRect r;
	r.x = x1;
	r.y = y1;
	r.w = std::max(0, x2 - x1);
	r.h = std::max(0, y2 - y1);
	return r;
}

void Graphics::ApplyTopClipRectToGL() {
	// Phase 3b：把当前裁剪栈顶（或空栈对应的全屏）写成 vkCmdSetScissor。
	// 没活动帧时早退（构造期 / shutdown 期 / -Debug 切场景瞬间都可能命中）。
	if (!m_vk || !m_vk->frameOpen) return;
	VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
	if (cb == VK_NULL_HANDLE) return;

	VkExtent2D sce = m_vk->ctx->SwapchainExtent();
	VkRect2D rect{};
	if (m_clipStack.empty()) {
		// 空栈 = 全逻辑画面。letterbox 下应限制到画面矩形（offset..offset+logical*scale），
		// 否则黑边区域可能被后续绘制写入。窗口模式 scale=1/offset=0 时即整个交换链。
		LogicalRectToFramebuffer(0, 0, m_windowWidth, m_windowHeight,
			rect.offset.x, rect.offset.y, rect.extent.width, rect.extent.height);
	}
	else {
		const ClipRect& cr = m_clipStack.back();
		// 逻辑坐标裁剪框 → 帧缓冲像素（scissor 不经 viewport 变换，必须手动套 letterbox）。
		int32_t  x; int32_t  y; uint32_t w; uint32_t h;
		LogicalRectToFramebuffer(cr.x, cr.y, cr.w, cr.h, x, y, w, h);
		if (x < 0) { if ((int32_t)w > -x) w += x; else w = 0; x = 0; }
		if (y < 0) { if ((int32_t)h > -y) h += y; else h = 0; y = 0; }
		// clamp to swapchain bounds
		if ((uint32_t)x > sce.width)  x = (int32_t)sce.width;
		if ((uint32_t)y > sce.height) y = (int32_t)sce.height;
		if (x + w > sce.width)  w = sce.width - x;
		if (y + h > sce.height) h = sce.height - y;
		rect.offset = { x, y };
		rect.extent = { w, h };
	}
	vkCmdSetScissor(cb, 0, 1, &rect);
}

void Graphics::PushClipRect(int x, int y, int w, int h) {
	if (tl_record) { RecordPushClipRect(*tl_record, x, y, w, h); return; }
	// 必须先把已积累的顶点刷出去，否则 push 之前 batched 的内容会被新的 scissor 一起裁剪。
	FlushBatch();
	FlushInstances();

	ClipRect rect;
	rect.x = x; rect.y = y; rect.w = w; rect.h = h;
	if (!m_clipStack.empty()) {
		rect = IntersectClip(m_clipStack.back(), rect);  // 嵌套取交集
	}
	m_clipStack.push_back(rect);
	ApplyTopClipRectToGL();
}

void Graphics::PopClipRect() {
	if (tl_record) { RecordPopClipRect(*tl_record); return; }
	if (m_clipStack.empty()) {
		LOG_WARN("Graphics") << "PopClipRect failed: stack underflow.";
		return;
	}
	// 在切换 scissor 状态之前先 flush，把当前 rect 下的顶点全部提交。
	FlushBatch();
	FlushInstances();
	m_clipStack.pop_back();
	// 栈空时也要把 scissor 重置回全屏，否则上一条 scissor 会继续影响后续画的内容。
	ApplyTopClipRectToGL();
}

void Graphics::FlushBatch() {
	// Phase 3b：把累积的 BatchVertex + glm::mat4 拷到当前帧的 host-visible VBO/SSBO，
	// 按 BatchVertex.blendMode 字段分段，每段 issue 一个 vkCmdDraw。
	// 没活动帧（未初始化、Begin 失败、析构期）时只清 CPU 缓冲。

	const size_t vertCount = m_batchVertices.size();
	const size_t matCount = m_batchMatrices.size();

	auto clearCpu = [&]() {
		m_batchVertices.clear();
		m_batchMatrices.clear();
		m_batchTextures.clear();
		};

	if (!m_vk || !m_vk->frameOpen || vertCount == 0) {
		clearCpu();
		return;
	}

	VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
	if (cb == VK_NULL_HANDLE) {
		clearCpu();
		return;
	}

	auto& fr = m_vk->frames[m_vk->frameIdx];

	const VkDeviceSize matBytes = matCount * sizeof(glm::mat4);
	const VkDeviceSize vertBytes = vertCount * sizeof(BatchVertex);

	if (fr.ssboCursor + matBytes > SSBO_BYTES_PER_FRAME ||
		fr.vboCursor + vertBytes > VBO_BYTES_PER_FRAME) {
		// 每帧只打一次警告——overflow 之后几乎每次 DrawXxx 都会再次 hit 这条路径，
		// 同步写控制台会把主线程憋成死循环（PVZ 在大场景下能堆到 60k+ 次/帧）。
		if (!fr.overflowWarned) {
			LOG_WARN("Graphics") << "Frame buffer overflow — ssbo " << fr.ssboCursor << "+" << matBytes
				<< "/" << SSBO_BYTES_PER_FRAME << "  vbo " << fr.vboCursor << "+" << vertBytes
				<< "/" << VBO_BYTES_PER_FRAME << " — dropping further draws this frame";
			fr.overflowWarned = true;
		}
		clearCpu();
		return;
	}

	// 1) 矩阵：本次 FlushBatch 的矩阵会从 ssboCursor 处开始写入；记下该位置对应的 mat4 下标，
	//    后面把它加到每条顶点的 matrixIndex 上，让 shader 看到的是 SSBO 内的绝对槽位。
	const uint32_t matrixBase = (uint32_t)(fr.ssboCursor / sizeof(glm::mat4));
	if (matCount > 0) {
		std::memcpy(static_cast<char*>(fr.ssbo->MappedPtr()) + fr.ssboCursor,
			m_batchMatrices.data(), (size_t)matBytes);
	}

	// 2) 顶点：matrixIndex += matrixBase，然后 memcpy 进 VBO
	for (auto& v : m_batchVertices) {
		v.matrixIndex += matrixBase;
	}
	const uint32_t firstVertex = (uint32_t)(fr.vboCursor / sizeof(BatchVertex));
	std::memcpy(static_cast<char*>(fr.vbo->MappedPtr()) + fr.vboCursor,
		m_batchVertices.data(), (size_t)vertBytes);

	fr.ssboCursor += matBytes;
	fr.vboCursor += vertBytes;

	// 3) 按 6 顶点一组（一个 quad）按 blendMode 字段分段，emit vkCmdDraw。
	const glm::mat4 projView = m_projection * m_viewMatrix;
	const VkDescriptorSet sets[2] = { fr.matrixSet, m_vk->texPool->DescriptorSet() };
	VkBuffer vbuf = fr.vbo->Handle();
	VkDeviceSize vboOff = 0;
	vkCmdBindVertexBuffers(cb, 0, 1, &vbuf, &vboOff);

	// Phase 5：分段 + vkCmdDraw 抽进静态 helper EmitDrawRange（同文件末尾），并行回放路径复用。
	// 这里直接从主线程 CPU 缓冲 m_batchVertices 读 blendMode（顺序遍历，cache 友好）。
	EmitDrawRange(cb, firstVertex, (uint32_t)vertCount,
		m_batchVertices.data(),
		m_vk->pipeBatchAlpha.get(), m_vk->pipeBatchAdd.get(),
		sets, projView);

	clearCpu();
}

void Graphics::FlushInstances() {
	if (m_batchInstances.empty()) return;
	if (!m_vk || !m_vk->frameOpen) {
		m_batchInstances.clear();
		return;
	}

	auto& fr = m_vk->frames[m_vk->frameIdx];
	const size_t needBytes = m_batchInstances.size() * sizeof(InstanceRecord);
	if (fr.instCursor + needBytes > INST_BYTES_PER_FRAME) {
		if (!fr.overflowWarned) {
			LOG_WARN("Graphics") << "FlushInstances overflow ("
				<< fr.instCursor << "+" << needBytes
				<< ">" << INST_BYTES_PER_FRAME << ")";
			fr.overflowWarned = true;
		}
		m_batchInstances.clear();
		return;
	}

	const uint32_t baseInstAbs = (uint32_t)(fr.instCursor / sizeof(InstanceRecord));
	std::memcpy(static_cast<char*>(fr.instBuf->MappedPtr()) + fr.instCursor,
		m_batchInstances.data(), needBytes);
	fr.instCursor += needBytes;

	VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
	if (cb == VK_NULL_HANDLE) { m_batchInstances.clear(); return; }

	const glm::mat4 projView = m_projection * m_viewMatrix;
	const VkDescriptorSet sets[2] = { fr.instSet, m_vk->texPool->DescriptorSet() };
	// 用 m_queuedInstanceBlend（实例队列私有状态）而非 m_currentBlendMode（被
	// DrawTexture/DrawTextureMatrix 等读作 per-vert bm 默认值的全局态）。这两者必须
	// 分离，否则 Animator 的 glow Append 会污染同帧后续 shadow 的 bm，把影子打成 Add
	// 不可见——参见 AppendReanimInstance 主线程分支的同条注释。
	pvz::VulkanPipeline* pipe = (m_queuedInstanceBlend == BlendMode::Add)
		? m_vk->pipeInstAdd.get()
		: m_vk->pipeInstAlpha.get();

	vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
	vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Layout(),
		0, 2, sets, 0, nullptr);
	vkCmdPushConstants(cb, pipe->Layout(), VK_SHADER_STAGE_VERTEX_BIT,
		0, sizeof(glm::mat4), &projView);
	vkCmdDraw(cb, 6, (uint32_t)m_batchInstances.size(), 0, baseInstAbs);

	m_batchInstances.clear();
}

int Graphics::BindTexture(uint32_t textureID) {
	// Phase 3b：textureID 现在就是 bindless 槽位索引（来自 VulkanTexturePool）。
	// 没有 32 单元限制，也不需要"已注册纹理表"——直接返回 ID 让 shader 用它 index 进 bindless 数组。
	// 留 m_batchTextures.push_back 是为了与原 push_back/clear 节奏兼容（CheckBatch 会读 size），
	// 不再用做查找表。
	m_batchTextures.push_back(textureID);
	return (int)textureID;
}

int Graphics::AddMatrix(const glm::mat4& matrix) {
	// Phase 3b：SSBO 容量足以装下整帧的矩阵，因此不再因为阈值触发 mid-frame flush。
	// 真正的 flush 边界由 PushClipRect / SetBlendMode / EndFrame 触发。
	m_batchMatrices.push_back(matrix);
	return (int)(m_batchMatrices.size() - 1);
}

void Graphics::AddVertices(const BatchVertex* vertices, int count) {
	// 主线程 cross-flush（worker 路径走 tl_record 直接进 slice 不经过这里）：
	// 如果 m_batchInstances 已积累记录，说明在本次 batch 写入之前 caller 调过
	// AppendReanimInstance。要保证 vkCmdDraw 顺序 == append 顺序（否则 instance
	// 会在 EndFrame 之前 mid-frame 被 blend-change 触发 flush，先于本批 batch 进入
	// cmd buffer，导致 lawn/UI 反盖住 plant body）——所以这里先 flush instance。
	// worker 模式不会走 main thread AddVertices；其 cmd-stream 由 ReplayAndEndParallel
	// 的 emitUpTo 按 SetBlend 段交错 emit，本来就保序。
	if (!m_batchInstances.empty()) {
		FlushInstances();
	}
	for (int i = 0; i < count; ++i) {
		m_batchVertices.push_back(vertices[i]);
	}
}

void Graphics::CheckBatch() {
	// Phase 3b（+3c 修复）：主动 flush 在还能把当前批 fit 进剩余空间时。
	// 阈值要早于"已经填满"——因为 FlushBatch 是把整个 m_batchVertices append 到本帧 buffer，
	// 一旦 m_batchVertices ≥ 剩余空间，flush 就会 drop（参见 FlushBatch 里的 overflow 报错）。
	// 同时给下一次 DrawXxx 留一份 6 顶点 + 1 mat4 的安全余量。
	const size_t vertBytes = m_batchVertices.size() * sizeof(BatchVertex);
	const size_t matBytes = m_batchMatrices.size() * sizeof(glm::mat4);

	VkDeviceSize vboLeft = VBO_BYTES_PER_FRAME;
	VkDeviceSize ssboLeft = SSBO_BYTES_PER_FRAME;
	if (m_vk && m_vk->frameOpen) {
		const auto& fr = m_vk->frames[m_vk->frameIdx];
		vboLeft = (fr.vboCursor < VBO_BYTES_PER_FRAME) ? (VBO_BYTES_PER_FRAME - fr.vboCursor) : 0;
		ssboLeft = (fr.ssboCursor < SSBO_BYTES_PER_FRAME) ? (SSBO_BYTES_PER_FRAME - fr.ssboCursor) : 0;
	}
	constexpr VkDeviceSize kRoom = 6 * sizeof(BatchVertex) + sizeof(glm::mat4); // ~328 B
	if (vertBytes + kRoom >= vboLeft || matBytes + kRoom >= ssboLeft) {
		FlushBatch();
	}
}

void Graphics::DrawTexture(const Texture* tex, float x, float y, float width, float height,
	float rotation, const glm::vec4& tint) {
	if (!tex) {
		LOG_WARN("Graphics") << "DrawTexture: null texture pointer";
		return;
	}
	if (tl_record) { RecordDrawTexture(*tl_record, tex, x, y, width, height, rotation, tint); return; }

	int texIndex = BindTexture(tex->id);

	// 构建局部变换矩阵：平移 -> 缩放 -> 旋转（绕中心）
	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	if (rotation != 0.0f) {
		local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
		local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
	}
	glm::mat4 finalMatrix = m_transformStack.back() * local;
	int matrixIndex = AddMatrix(finalMatrix);

	// 转换颜色为 0-1 格式
	glm::vec4 normalizedTint = NormalizeColor(tint);

	// 构建两个三角形（6个顶点）的批处理顶点数据
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;
	BatchVertex vertices[6] = {
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b , normalizedTint.a, bm},
		{1.0f, 1.0f, 1.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g , normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b , normalizedTint.a, bm},
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r , normalizedTint.g, normalizedTint.b , normalizedTint.a, bm},
		{0.0f, 0.0f, 0.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b , normalizedTint.a, bm}
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

void Graphics::DrawTextureMatrix(const Texture* tex, const glm::mat4& transform,
	float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode) {
	if (!tex) return;
	if (tl_record) { RecordDrawTextureMatrix(*tl_record, tex, transform, pivotX, pivotY, tint, blendMode); return; }

	// 图集重映射：若该纹理已被打进图集页，则改绑图集页并把 UV 收窄到子矩形
	const Texture* bindTex = tex;
	float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
	if (tex->atlasPage) {
		bindTex = tex->atlasPage;
		u0 = tex->aU0; v0 = tex->aV0; u1 = tex->aU1; v1 = tex->aV1;
	}
	int texIndex = BindTexture(bindTex->id);

	glm::mat4 pivotTransform;
	if (pivotX != 0.0f || pivotY != 0.0f) {
		glm::vec3 pivot(pivotX, pivotY, 0.0f);
		pivotTransform = glm::translate(glm::mat4(1.0f), pivot) *
			transform *
			glm::translate(glm::mat4(1.0f), -pivot);
	}
	else {
		pivotTransform = transform;
	}
	// 栈顶为单位阵时跳过 mat4×mat4（Animator::Draw 压入单位阵，~9万 sprite/帧的串行热点）
	glm::mat4 finalMatrix = m_transformIsIdentity.back()
		? pivotTransform
		: (m_transformStack.back() * pivotTransform);
	int matrixIndex = AddMatrix(finalMatrix);

	// 转换颜色为 0-1 格式
	glm::vec4 normalizedTint = NormalizeColor(tint);

	// 确定实际混合模式：None 表示使用当前全局混合模式
	BlendMode actualMode = (blendMode == BlendMode::None) ? m_currentBlendMode : blendMode;
	float bm = (actualMode == BlendMode::Add) ? 1.0f : 0.0f;

	BatchVertex vertices[6] = {
		{0.0f, 1.0f, u0, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 1.0f, u1, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, u1, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{0.0f, 1.0f, u0, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{0.0f, 0.0f, u0, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, u1, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm}
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

void Graphics::AppendReanimInstance(const InstanceRecord& rec, BlendMode blendMode) {
	// Worker thread path: write into slot's slice
	if (tl_record) {
		WorkerRecord& r = *tl_record;
		VkWorkerSlice& sl = r.slice;

		if (sl.instCount >= sl.instCap) {
			sl.overflowed = true;
			return;
		}

		// Save/restore tl_blend，与 RecordDrawTextureMatrix(行 1897-1951) 的模式对齐。
		// 不恢复会污染同 slot 后续 Record* 调用——典型表现是 Animator 画完 glow（Add）后，
		// 同一 worker 的 ShadowComponent::Draw 把 bm 字段（行 1893 读 tl_blend）写成 1.0，
		// 影子被 Add 混合打成不可见。SetBlend cmd 必须先于 instance 数据写入，以便
		// instOffsetAtCmd 正确捕获"blend 切换时的 instance 计数"——回放按它分段切 pipeline。
		const BlendMode savedBlend = tl_blend;
		const bool needSwitch = (blendMode != tl_blend);
		if (needSwitch) {
			RecordSetBlendMode(r, blendMode);
		}

		sl.instPtr[sl.instCount++] = rec;

		if (needSwitch) {
			RecordSetBlendMode(r, savedBlend);
		}
		return;
	}

	// Main thread serial path——两个独立修复：
	//
	// (1) Cross-flush：如果 m_batchVertices 已积累 batch quads（典型是先画 lawn/shadow/UI
	//     再画 plant），先 FlushBatch 让那批 vkCmdDraw 进 cmd buffer，再让本次 instance
	//     append 接在后面。否则后续 mid-frame FlushInstances（由 blend 切换触发）会先于
	//     m_batchVertices 的 EndFrame flush 进 cmd buffer，造成 plant body 被 lawn 反盖
	//     不可见的 Z-order bug。worker 路径有 emitUpTo 按段交错回放，本身保序，无需此 fix。
	//
	// (2) Blend 状态用独立的 m_queuedInstanceBlend，**不再触碰** m_currentBlendMode。
	//     原始代码用 m_currentBlendMode 同时承担两个不变量：
	//       (a) 实例队列当前堆的 blend（FlushInstances 据此选 pipeline）
	//       (b) DrawTexture/DrawTextureMatrix 的 per-vert bm 默认值
	//     一旦 glow Append 把它改成 Add，下游所有 (b) 的消费者（典型是 ShadowComponent）
	//     就被污染——shadow 顶点 bm=1，整段走 pipeBatchAdd 被打成不可见。
	//     分离后 flush 计数与原始代码字节级一致，行为不变，只切断了污染路径。
	if (!m_batchVertices.empty()) {
		FlushBatch();
	}
	if (blendMode != m_queuedInstanceBlend) {
		FlushInstances();
		m_queuedInstanceBlend = blendMode;
	}
	if ((int)m_batchInstances.size() >= m_batchInstancesLimit) {
		FlushInstances();
	}
	m_batchInstances.push_back(rec);
}

void Graphics::DrawTextureRegion(const Texture* tex,
	float srcX, float srcY, float srcW, float srcH,
	float dstX, float dstY, float dstW, float dstH,
	float rotation, const glm::vec4& tint)
{
	if (!tex || tex->id == 0) return;
	if (tl_record) { RecordDrawTextureRegion(*tl_record, tex, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH, rotation, tint); return; }

	// 计算归一化 UV 坐标
	float u0 = srcX / tex->width;
	float v0 = srcY / tex->height;
	float u1 = (srcX + srcW) / tex->width;
	float v1 = (srcY + srcH) / tex->height;

	// 构建局部变换矩阵（平移 -> 缩放 -> 绕中心旋转）
	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(dstX, dstY, 0.0f));
	local = glm::scale(local, glm::vec3(dstW, dstH, 1.0f));
	if (rotation != 0.0f) {
		local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
		local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
	}

	int texIndex = BindTexture(tex->id);
	glm::mat4 finalMatrix = m_transformStack.back() * local;
	int matrixIndex = AddMatrix(finalMatrix);

	// 转换颜色为 0-1 格式
	glm::vec4 normalizedTint = NormalizeColor(tint);

	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;
	BatchVertex vertices[6] = {
		{0.0f, 1.0f, u0, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 1.0f, u1, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, u1, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{0.0f, 1.0f, u0, v1, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{0.0f, 0.0f, u0, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
		{1.0f, 0.0f, u1, v0, (uint32_t)texIndex, (uint32_t)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm}
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

// 文字超采样：letterbox 把整幅逻辑画面放大 letterboxScale 倍铺满全屏。文字若按逻辑
// fontSize 光栅化再被 GPU 放大就会糊。改为按物理像素 physSize 光栅化，绘制时再按
// superSample 除回逻辑尺寸——屏幕布局不变、像素密度匹配屏幕。窗口模式 scale=1 → 恒等。
static int ComputeTextRasterSize(int fontSize, float letterboxScale, float& outSuperSample) {
	int phys = (int)std::lround(fontSize * (double)letterboxScale);
	if (phys < 1) phys = 1;
	outSuperSample = (float)phys / (float)fontSize;
	return phys;
}

uint32_t Graphics::GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color, int& outWidth, int& outHeight, float& outSuperSample) {
	// 按物理像素超采样光栅化。physSize 进缓存键，避免窗口/全屏两种密度的同一句文字串号。
	float superSample;
	const int physSize = ComputeTextRasterSize(fontSize, m_letterboxScale, superSample);
	outSuperSample = superSample;

	// 生成缓存键
	std::stringstream ss;
	ss << text << "|" << fontKey << "|" << physSize << "|"
		<< (int)color.r << "," << (int)color.g << "," << (int)color.b << "," << (int)color.a;
	std::string key = ss.str();

	auto it = m_textCache.find(key);
	if (it != m_textCache.end()) {
		// 命中缓存：移到链表头部（最近使用）
		m_textCacheOrder.erase(it->second.second);
		m_textCacheOrder.push_front(key);
		it->second.second = m_textCacheOrder.begin();
		outWidth = it->second.first.width;
		outHeight = it->second.first.height;
		return it->second.first.textureID;
	}

	CachedText entry;
	if (!RenderTextToVulkanTexture(text, fontKey, physSize, color, entry)) {
		return 0;
	}
	entry.superSample = superSample;
	outWidth = entry.width;
	outHeight = entry.height;

	// 超出容量时淘汰最久未使用的条目
	if ((int)m_textCache.size() >= TEXT_CACHE_MAX_SIZE) {
		const std::string& lruKey = m_textCacheOrder.back();
		auto lruIt = m_textCache.find(lruKey);
		if (lruIt != m_textCache.end()) {
			// Phase 3c：回收 bindless 槽位（否则池会被反复创建-淘汰的文字撑爆）。
			if (m_vk && m_vk->texPool && lruIt->second.first.vkTex) {
				m_vk->texPool->DestroyTexture(lruIt->second.first.vkTex);
			}
			m_textCache.erase(lruIt);
		}
		m_textCacheOrder.pop_back();
	}

	// 插入新条目到链表头部
	m_textCacheOrder.push_front(key);
	m_textCache[key] = { entry, m_textCacheOrder.begin() };
	return entry.textureID;
}

bool Graphics::RenderTextToVulkanTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color, CachedText& out) {
	// Phase 3c：TTF → SDL_Surface → ABGR8888 → VulkanTexturePool::CreateTextureRGBA8。
	// 失败要让 out 保持空（textureID=0），上层会据此 early-return 不显示。
	if (!m_vk || !m_vk->texPool) {
		LOG_ERROR("Graphics") << "RenderTextToVulkanTexture: Vulkan 尚未初始化";
		return false;
	}
	if (text.empty()) return false;

	TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
	if (!font) {
		LOG_WARN("Graphics") << "RenderTextToVulkanTexture: 找不到字体: " << fontKey
			<< " size=" << fontSize;
		return false;
	}

	const SDL_Color sdlColor = ToSDLColor(color);
	SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), sdlColor);
	if (!surface) {
		LOG_ERROR("Graphics") << "RenderTextToVulkanTexture: TTF_RenderUTF8_Blended 失败: "
			<< TTF_GetError();
		return false;
	}

	// 统一到 ABGR8888，匹配 VulkanTexturePool 的 VK_FORMAT_R8G8B8A8_UNORM 输入约定。
	SDL_Surface* conv = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(surface);
	if (!conv) {
		LOG_ERROR("Graphics") << "RenderTextToVulkanTexture: SDL_ConvertSurfaceFormat 失败: "
			<< SDL_GetError();
		return false;
	}

	pvz::VulkanTexture* vkt = m_vk->texPool->CreateTextureRGBA8(conv->w, conv->h, conv->pixels);
	const int w = conv->w;
	const int h = conv->h;
	SDL_FreeSurface(conv);
	if (!vkt) {
		LOG_ERROR("Graphics") << "RenderTextToVulkanTexture: VulkanTexturePool 上传失败";
		return false;
	}

	out.textureID = vkt->bindlessIndex;
	out.width = w;
	out.height = h;
	out.vkTex = vkt;
	return true;
}

CachedText Graphics::AcquireTextTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color) {
	// 与 GetOrCreateTextTexture 同款超采样：按物理像素光栅化，physSize 进键。
	float superSample;
	const int physSize = ComputeTextRasterSize(fontSize, m_letterboxScale, superSample);

	std::stringstream ss;
	ss << text << "|" << fontKey << "|" << physSize << "|"
		<< (int)color.r << "," << (int)color.g << "," << (int)color.b << "," << (int)color.a;
	std::string key = ss.str();

	auto it = m_pinnedTextCache.find(key);
	if (it != m_pinnedTextCache.end()) {
		return it->second;
	}

	// RenderTextToVulkanTexture 走 VulkanTexturePool 上传，且要写共享的
	// m_pinnedTextCache——两者都只能在主线程做。worker 线程
	// （tl_record 非空，例如并行 Draw 录制阶段）上未命中时直接放弃本帧返回空
	// 句柄，由主线程预热缓存后即稳定命中。缺少此守卫会在 worker 线程拿到无效/
	// 串号的纹理 ID（表现为数字变成贴图或别的文字），并发写 map 还会损坏容器。
	if (tl_record) return {};

	CachedText entry;
	if (!RenderTextToVulkanTexture(text, fontKey, physSize, color, entry)) {
		return {};
	}
	entry.superSample = superSample;
	entry.generation = m_textGeneration;
	m_pinnedTextCache.emplace(std::move(key), entry);
	return entry;
}

void Graphics::DrawCachedText(const CachedText& handle, float x, float y, float scale) {
	if (handle.textureID == 0) return;
	// 过期句柄（底层纹理已被 ClearPinnedTextCache 销毁）直接丢弃，避免读悬垂的 bindless 槽位。
	// 消费者应在主线程经 IsCachedTextStale 重新获取；这里只做防御。
	if (handle.generation != m_textGeneration) return;
	if (tl_record) { RecordDrawCachedText(*tl_record, handle, x, y, scale); return; }

	int texIndex = BindTexture(handle.textureID);
	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	// 超采样纹理：除回逻辑尺寸保证屏幕布局不变（handle.superSample=1 时等价）。
	const float inv = scale / handle.superSample;
	local = glm::scale(local, glm::vec3(handle.width * inv, handle.height * inv, 1.0f));
	glm::mat4 finalMatrix = m_transformStack.back() * local;
	int matrixIndex = AddMatrix(finalMatrix);

	BatchVertex vertices[6] = {
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 1.0f, 1.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{0.0f, 0.0f, 0.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f}
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

void Graphics::ClearPinnedTextCache() {
	// Phase 3c：把每条 pinned 文字纹理还给 pool（要求 pool 仍存活——GameApp::Shutdown 顺序保证了）。
	if (m_vk && m_vk->texPool) {
		for (auto& kv : m_pinnedTextCache) {
			if (kv.second.vkTex) m_vk->texPool->DestroyTexture(kv.second.vkTex);
		}
	}
	m_pinnedTextCache.clear();
	// 代际号递增：所有在外部持有的旧句柄副本就此失效（IsCachedTextStale 会返回 true），
	// 防止消费者拿已销毁的 bindless 槽位去绘制。
	++m_textGeneration;
}

void Graphics::DrawText(const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale) {
	// worker：defer 到主线程，onTop=false → 回放时就地交错渲染（与对象同 z-order）。
	if (tl_record) { RecordDrawText(*tl_record, text, fontKey, fontSize, color, x, y, scale, /*onTop*/false); return; }

	// 主线程串行：先 flush 已排队的 reanim instance（草坪/影子/前面对象 sprite），文字随后写入
	// batch，将在下一次 cross-flush / EndFrame 落在它们之上——即"当前层"（在已画对象之上、后续
	// 对象之下）。否则 AppendReanimInstance 的 cross-flush 会把 batch 压到后续 instance 下层 → 隐身。
	// 对象循环之外的调用（UI/菜单文字）此刻 instance 队列为空，FlushInstances 是 no-op，行为不变。
	FlushInstances();

	int w, h;
	float superSample;
	uint32_t texID = GetOrCreateTextTexture(text, fontKey, fontSize, color, w, h, superSample);
	if (texID == 0) return;

	int texIndex = BindTexture(texID);
	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	// 纹理按物理像素超采样，除回逻辑尺寸保证屏幕布局不变（superSample=1 时等价）。
	const float inv = scale / superSample;
	local = glm::scale(local, glm::vec3(w * inv, h * inv, 1.0f));
	glm::mat4 finalMatrix = m_transformStack.back() * local;
	int matrixIndex = AddMatrix(finalMatrix);

	BatchVertex vertices[6] = {
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 1.0f, 1.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{0.0f, 0.0f, 0.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, 1,1,1,1, 0.0f}
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

void Graphics::DrawTextOnTop(const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale) {
	// worker 线程：onTop=true → ReplayAndEndParallel 在所有 slot 几何之后统一渲染 = 绝对顶层。
	if (tl_record) {
		RecordDrawText(*tl_record, text, fontKey, fontSize, color, x, y, scale, /*onTop*/true);
		return;
	}
	// 主线程串行：无并行回放阶段、无法真正延迟到所有对象之后；这里把已排队的 batch + instance
	// 全部 flush，再绘制，等价于"当前已绘制内容之上"。串行对象少，足够近似顶层。
	if (!m_batchVertices.empty()) FlushBatch();
	FlushInstances();
	DrawText(text, fontKey, fontSize, color, x, y, scale);
}

void Graphics::ClearTextCache() {
	// Phase 3c：把每条 LRU 文字纹理还给 pool。pool 不在了就直接 clear（构造/Shutdown 期都安全）。
	if (m_vk && m_vk->texPool) {
		for (auto& kv : m_textCache) {
			if (kv.second.first.vkTex) m_vk->texPool->DestroyTexture(kv.second.first.vkTex);
		}
	}
	m_textCache.clear();
	m_textCacheOrder.clear();
}

void Graphics::SetClearColor(float r, float g, float b, float a) {
	// Phase 3a：保存归一化到 [0,1]，每帧由 VulkanRenderer::BeginFrame 读取。
	m_clearR = r / 255.0f;
	m_clearG = g / 255.0f;
	m_clearB = b / 255.0f;
	m_clearA = a / 255.0f;
}

void Graphics::SetBlendMode(BlendMode mode) {
	if (tl_record) { RecordSetBlendMode(*tl_record, mode); return; }

	if (m_currentBlendMode == mode) return;

	m_currentBlendMode = mode;

	// 纹理批次通过逐顶点 blendMode 字段在 FlushBatch 时分段处理，pipeline 切换在 emit 时根据
	// blendMode 决定，无需在这里做 GL 状态切换。
}

void Graphics::Clear() {
	// Phase 3a：真正的清屏由 VulkanRenderer::BeginFrame 完成；这里只做 CPU 侧的卫生检查。
	if (!m_clipStack.empty()) {
		LOG_WARN("Graphics") << "Unbalanced PushClipRect/PopClipRect across frames ("
			<< m_clipStack.size() << " entries left); resetting.";
		m_clipStack.clear();
	}
}

void Graphics::UpdateViewMatrix() {
	m_viewMatrix = glm::mat4(1.0f);
	m_viewMatrix = glm::scale(m_viewMatrix, glm::vec3(m_cameraZoom, m_cameraZoom, 1.0f));
	if (m_cameraRotation != 0.0f)
		m_viewMatrix = glm::rotate(m_viewMatrix, glm::radians(-m_cameraRotation), glm::vec3(0.0f, 0.0f, 1.0f));
	m_viewMatrix = glm::translate(m_viewMatrix, glm::vec3(-m_cameraPos.x, -m_cameraPos.y, 0.0f));
}

void Graphics::SetCameraPosition(float x, float y) {
	m_cameraPos = glm::vec2(x, y);
	UpdateViewMatrix();
}

void Graphics::MoveCamera(float dx, float dy) {
	m_cameraPos += glm::vec2(dx, dy);
	UpdateViewMatrix();
}

void Graphics::SetCameraZoom(float zoom) {
	m_cameraZoom = zoom;
	UpdateViewMatrix();
}

void Graphics::SetCameraRotation(float degrees) {
	m_cameraRotation = degrees;
	UpdateViewMatrix();
}

void Graphics::ResetCamera() {
	m_cameraPos = glm::vec2(0.0f);
	m_cameraZoom = 1.0f;
	m_cameraRotation = 0.0f;
	m_viewMatrix = glm::mat4(1.0f);
}

void Graphics::RecomputeLetterbox() {
	// 真实交换链尺寸。m_vk 未就绪（构造期/无 Vulkan）时退化为逻辑尺寸 → scale=1、offset=0。
	float realW = (float)m_windowWidth;
	float realH = (float)m_windowHeight;
	if (m_vk && m_vk->ctx) {
		VkExtent2D ext = m_vk->ctx->SwapchainExtent();
		if (ext.width > 0 && ext.height > 0) {
			realW = (float)ext.width;
			realH = (float)ext.height;
		}
	}
	const float prevScale = m_letterboxScale;
	// 等比：取较小的轴缩放比，保证整幅逻辑画面装得下；剩余空间均分为两侧黑边。
	m_letterboxScale = std::min(realW / (float)m_windowWidth, realH / (float)m_windowHeight);
	m_letterboxOffsetX = (realW - m_windowWidth * m_letterboxScale) * 0.5f;
	m_letterboxOffsetY = (realH - m_windowHeight * m_letterboxScale) * 0.5f;

	// 缩放比变了（窗口⇄全屏切换）→ 已缓存的文字纹理是按旧密度光栅化的，立即失效让其按新
	// 密度重建，否则全屏后文字会沿用低密度纹理而发糊。一次性重光栅化（非每帧），代价可接受。
	if (std::abs(m_letterboxScale - prevScale) > 1e-4f) {
		ClearTextCache();
		ClearPinnedTextCache();
	}
}

void Graphics::LogicalRectToFramebuffer(int lx, int ly, int lw, int lh,
	int32_t& outX, int32_t& outY, uint32_t& outW, uint32_t& outH) const {
	// scissor 走帧缓冲像素、不经 viewport 变换，所以这里必须手动套 letterbox 缩放+偏移。
	outX = (int32_t)std::lround(lx * m_letterboxScale + m_letterboxOffsetX);
	outY = (int32_t)std::lround(ly * m_letterboxScale + m_letterboxOffsetY);
	outW = (uint32_t)std::max(0L, std::lround(lw * m_letterboxScale));
	outH = (uint32_t)std::max(0L, std::lround(lh * m_letterboxScale));
}

glm::vec2 Graphics::LogicalToWorld(float logicalX, float logicalY) const {
	// 入参是「逻辑坐标」(0..1100/0..600)，不是帧缓冲像素——
	// 全场 15+ 调用点绝大多数传的是 UI 布局坐标用于绘制（Button/Slider/ShovelBank 等）。
	// 鼠标的 letterbox 逆变换在 InputHandler::ProcessEvent 入口已完成，故此处不做 letterbox，
	// 只保留 逻辑坐标 → 世界坐标 的相机逆变换。窗口/全屏行为一致。
	// 逻辑坐标 → NDC（注意 Y 轴翻转，因为正交投影 top=0）
	float ndcX = (logicalX / m_windowWidth) * 2.0f - 1.0f;
	float ndcY = 1.0f - (logicalY / m_windowHeight) * 2.0f;
	glm::vec4 worldPos = glm::inverse(m_projection * m_viewMatrix) * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
	return glm::vec2(worldPos.x, worldPos.y);
}

glm::vec2 Graphics::WorldToLogical(float worldX, float worldY) const {
	glm::vec4 clipPos = m_projection * m_viewMatrix * glm::vec4(worldX, worldY, 0.0f, 1.0f);
	// NDC → 逻辑坐标（Y 轴翻转）
	float logicalX = (clipPos.x + 1.0f) * 0.5f * m_windowWidth;
	float logicalY = (1.0f - clipPos.y) * 0.5f * m_windowHeight;
	return glm::vec2(logicalX, logicalY);
}

void Graphics::DrawLine(float x1, float y1, float x2, float y2, const glm::vec4& color) {
	if (tl_record) { RecordDrawLine(*tl_record, x1, y1, x2, y2, color); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	// 线段转为 1px 宽四边形，走纹理批次（1×1 白纹理）保证绘制顺序
	const glm::mat4& transform = m_transformStack.back();
	glm::vec4 p0 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
	glm::vec4 p1 = transform * glm::vec4(x2, y2, 0.0f, 1.0f);

	float dx = p1.x - p0.x, dy = p1.y - p0.y;
	float len = sqrtf(dx * dx + dy * dy);
	if (len < 0.001f) return;
	float nx = -dy / len * 0.5f, ny = dx / len * 0.5f;

	int texIndex = BindTexture(m_whiteTexture);
	int matIndex = AddMatrix(glm::mat4(1.0f));
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;

	BatchVertex verts[6] = {
		{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		{p0.x - nx, p0.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		{p1.x + nx, p1.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
	};
	AddVertices(verts, 6);
	CheckBatch();
}

void Graphics::DrawRect(float x, float y, float width, float height, const glm::vec4& color) {
	if (tl_record) { RecordDrawRect(*tl_record, x, y, width, height, color); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	// 4 条边各转为 1px 宽四边形，走纹理批次
	const glm::mat4& transform = m_transformStack.back();
	glm::vec4 p[4] = {
		transform * glm::vec4(x,         y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y + height, 0.0f, 1.0f),
		transform * glm::vec4(x,         y + height, 0.0f, 1.0f),
	};

	int texIndex = BindTexture(m_whiteTexture);
	int matIndex = AddMatrix(glm::mat4(1.0f));
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < 4; ++i) {
		float ax = p[i].x, ay = p[i].y;
		float bx = p[(i + 1) % 4].x, by = p[(i + 1) % 4].y;
		float edx = bx - ax, edy = by - ay;
		float len = sqrtf(edx * edx + edy * edy);
		if (len < 0.001f) continue;
		float nx = -edy / len * 0.5f, ny = edx / len * 0.5f;

		BatchVertex verts[6] = {
			{ax + nx, ay + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{ax - nx, ay - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{bx - nx, by - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{ax + nx, ay + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{bx - nx, by - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{bx + nx, by + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		};
		AddVertices(verts, 6);
	}
	CheckBatch();
}

void Graphics::FillRect(float x, float y, float width, float height, const glm::vec4& color) {
	if (tl_record) { RecordFillRect(*tl_record, x, y, width, height, color); return; }

	// 使用 1×1 白色纹理加入纹理批次，保证与其他纹理的绘制顺序正确
	int texIndex = BindTexture(m_whiteTexture);

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	glm::mat4 finalMatrix = m_transformStack.back() * local;
	int matrixIndex = AddMatrix(finalMatrix);

	glm::vec4 nc = NormalizeColor(color);
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;
	BatchVertex vertices[6] = {
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		{1.0f, 1.0f, 1.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		{0.0f, 1.0f, 0.0f, 1.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		{0.0f, 0.0f, 0.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		{1.0f, 0.0f, 1.0f, 0.0f, (uint32_t)texIndex, (uint32_t)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
	};
	AddVertices(vertices, 6);
	CheckBatch();
}

void Graphics::DrawCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	if (tl_record) { RecordDrawCircle(*tl_record, cx, cy, radius, color, segments); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	// 每段弧转为 1px 宽四边形，走纹理批次
	const glm::mat4& transform = m_transformStack.back();

	int texIndex = BindTexture(m_whiteTexture);
	int matIndex = AddMatrix(glm::mat4(1.0f));
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		float angle0 = 2.0f * glm::pi<float>() * i / segments;
		float angle1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		glm::vec4 p0 = transform * glm::vec4(cx + radius * cosf(angle0), cy + radius * sinf(angle0), 0.0f, 1.0f);
		glm::vec4 p1 = transform * glm::vec4(cx + radius * cosf(angle1), cy + radius * sinf(angle1), 0.0f, 1.0f);

		float dx = p1.x - p0.x, dy = p1.y - p0.y;
		float len = sqrtf(dx * dx + dy * dy);
		if (len < 0.001f) continue;
		float nx = -dy / len * 0.5f, ny = dx / len * 0.5f;

		BatchVertex verts[6] = {
			{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p0.x - nx, p0.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p1.x + nx, p1.y + ny, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		};
		AddVertices(verts, 6);
	}
	CheckBatch();
}

void Graphics::FillCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	if (tl_record) { RecordFillCircle(*tl_record, cx, cy, radius, color, segments); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	// 展开三角扇，走纹理批次
	const glm::mat4& transform = m_transformStack.back();
	glm::vec4 center = transform * glm::vec4(cx, cy, 0.0f, 1.0f);

	int texIndex = BindTexture(m_whiteTexture);
	int matIndex = AddMatrix(glm::mat4(1.0f));
	float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		float angle0 = 2.0f * glm::pi<float>() * i / segments;
		float angle1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		glm::vec4 p0 = transform * glm::vec4(cx + radius * cosf(angle0), cy + radius * sinf(angle0), 0.0f, 1.0f);
		glm::vec4 p1 = transform * glm::vec4(cx + radius * cosf(angle1), cy + radius * sinf(angle1), 0.0f, 1.0f);

		BatchVertex verts[3] = {
			{center.x, center.y, 0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p0.x, p0.y,         0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
			{p1.x, p1.y,         0.5f, 0.5f, (uint32_t)texIndex, (uint32_t)matIndex, r,g,b,a, bm},
		};
		AddVertices(verts, 3);
	}
	CheckBatch();
}

// ============================================================================
//  多线程录制 / 回放
// ============================================================================
//
// 关键不变量：
//   1. tl_record 只在 worker 线程上非空，主线程上始终为 nullptr。
//   2. 每个 slot 一份的 WorkerRecord / WorkerThreadState 由该 slot 绑定的 worker
//      线程独占写入，无 lock 必要。
//   3. Replay 在主线程串行执行；走的是和原来一样的 BindTexture / AddMatrix /
//      AddVertices / CheckBatch / FlushBatch 路径，所以 GL 行为与原顺序提交一致。
//   4. 顺序保证：ThreadPool 按 idx 把 [idx*chunk, idx*chunk+chunk) 分给 worker
//      idx，slot 也取这个 idx。回放循环 slot = 0..N-1，slot 内按 cmd push 顺序
//      重放，整体绝对顺序与原串行 Draw 等价。

void Graphics::BeginParallelRecord(int numWorkers) {
	if (numWorkers <= 0) {
		m_numActiveWorkers = 0;
		return;
	}

	// Phase 5：先把主线程预先累积的 CPU 批排空到 GPU。这样切片区域起始游标干净，
	// 主线程 UI 既有数据落在并行区前面，回放后续的 text spill 落在后面。
	FlushBatch();
	FlushInstances();  // Task 4: 同步刷新主线程 instance 缓冲，保证 instCursor 在并行区起始干净

	// 确保 records 与 states 都有 numWorkers 份
	if ((int)m_workerRecords.size() < numWorkers) m_workerRecords.resize(numWorkers);
	if ((int)m_workerStates.size() < numWorkers) m_workerStates.resize(numWorkers);

	// 当前 Graphics 状态作为每个 worker 的初始基线
	const glm::mat4& curTop = m_transformStack.back();
	const bool       curId = m_transformIsIdentity.back() != 0;

	// 默认把所有 slot 的切片置为"零容量 / 空指针"——SliceHasRoom 会直接拒绝写入，
	// 工作线程不会 crash。下面如果检测到帧合法 + 容量充足，会重新填充实际指针。
	for (int i = 0; i < numWorkers; ++i) {
		WorkerRecord& r = m_workerRecords[i];
		r.Reset();
		if (r.cmds.capacity() == 0) {
			r.cmds.reserve(16 * 1024);
			r.clips.reserve(8);
			r.blendModes.reserve(16);
			r.textCmds.reserve(64);
		}
		r.slice = VkWorkerSlice{};  // 全部归零
		r.initialTopTransform = curTop;
		r.initialTopIsIdentity = curId;
		r.initialClipStack = m_clipStack;
		r.initialBlend = m_currentBlendMode;
	}
	m_parallelBaseVbo = m_parallelBaseSsbo = m_parallelBaseInst = 0;
	m_parallelVboBytes = m_parallelSsboBytes = m_parallelInstBytes = 0;
	m_numActiveWorkers = numWorkers;  // 永远 set —— 调用方（GameObjectManager）依赖此值 dispatch

	// 没活动帧（构造期 / shutdown 期 / 切场景瞬间）就跳过切片填充——
	// SliceHasRoom 在 worker 端会拒绝所有写入，replay 也会早退。
	if (!m_vk || !m_vk->frameOpen) return;
	auto& fr = m_vk->frames[m_vk->frameIdx];

	const uint64_t baseVbo = (uint64_t)fr.vboCursor;
	const uint64_t baseSsbo = (uint64_t)fr.ssboCursor;
	const uint64_t baseInst = (uint64_t)fr.instCursor;
	const uint64_t vboRemain = (VBO_BYTES_PER_FRAME > baseVbo) ? (VBO_BYTES_PER_FRAME - baseVbo) : 0;
	const uint64_t ssboRemain = (SSBO_BYTES_PER_FRAME > baseSsbo) ? (SSBO_BYTES_PER_FRAME - baseSsbo) : 0;
	const uint64_t instRemain = (INST_BYTES_PER_FRAME > baseInst) ? (INST_BYTES_PER_FRAME - baseInst) : 0;

	// 给并行区域之后的主线程绘制留点余量：deferred text、post-parallel UI、debug overlay 之类。
	// 之前出现过 slice 把整段 buffer 吃光、deferred text 再走 FlushBatch 时 1 字节没剩立刻 overflow
	// 的情况。8MB VBO / 1MB SSBO 足够装一帧的 UI + text；占整段 64MB / 16MB 的 12.5% / 6.25%，
	// 损失的并行写入容量可以忽略（4400 僵尸场景实测 ~17MB 顶点，并行区 56MB 仍然有 3× 余量）。
	constexpr uint64_t POST_PARALLEL_RESERVE_VBO = 4u * 1024u * 1024u;
	constexpr uint64_t POST_PARALLEL_RESERVE_SSBO = 1u * 1024u * 1024u;
	constexpr uint64_t POST_PARALLEL_RESERVE_INST = 1u * 1024u * 1024u;
	const uint64_t vboUsable = (vboRemain > POST_PARALLEL_RESERVE_VBO) ? (vboRemain - POST_PARALLEL_RESERVE_VBO) : 0;
	const uint64_t ssboUsable = (ssboRemain > POST_PARALLEL_RESERVE_SSBO) ? (ssboRemain - POST_PARALLEL_RESERVE_SSBO) : 0;
	const uint64_t instUsable = (instRemain > POST_PARALLEL_RESERVE_INST) ? (instRemain - POST_PARALLEL_RESERVE_INST) : 0;

	// 硬门槛：连"每 worker 一个 stride 的等分"都放不下，说明本帧已被主线程画满。
	// 与旧逻辑语义一致——slot 切片保持零容量，worker 写入会被静默丢弃。
	const uint64_t equalVboBytes = (vboUsable / numWorkers / sizeof(BatchVertex)) * sizeof(BatchVertex);
	const uint64_t equalSsboBytes = (ssboUsable / numWorkers / sizeof(glm::mat4)) * sizeof(glm::mat4);
	const uint64_t equalInstBytes = (instUsable / numWorkers / sizeof(InstanceRecord)) * sizeof(InstanceRecord);
	if (equalVboBytes == 0 || equalSsboBytes == 0 || equalInstBytes == 0) {
		// 每帧只打一次警告（fr.overflowWarned 是 FlushBatch overflow 共用的旗，足够）。
		if (!fr.overflowWarned) {
			LOG_WARN("Graphics") << "BeginParallelRecord: insufficient frame buffer capacity ("
				<< "vbo " << baseVbo << "/" << VBO_BYTES_PER_FRAME
				<< ", ssbo " << baseSsbo << "/" << SSBO_BYTES_PER_FRAME
				<< ", inst " << baseInst << "/" << INST_BYTES_PER_FRAME << ")";
			fr.overflowWarned = true;
		}
		return;
	}

	// 自适应加权切片：用上一并行帧各 slot 的占用作权重，按比例瓜分 usable。
	// 每个 slot 先保底一块地板（让上帧空、本帧突然变重的 slot 不至于 0 容量直接丢，
	// 撑过一帧后权重就会把它放大）；剩余弹性容量按权重分配。Σ ≤ usable 由构造保证。
	// 无历史（首并行帧 / numWorkers 变化）则退回等分，1~2 帧内自动收敛。
	auto floorTo = [](uint64_t v, uint64_t stride) { return (v / stride) * stride; };
	const uint64_t VSTRIDE = sizeof(BatchVertex);
	const uint64_t MSTRIDE = sizeof(glm::mat4);
	const uint64_t ISTRIDE = sizeof(InstanceRecord);
	const uint64_t minVbo = floorTo(2048ull * VSTRIDE, VSTRIDE);  // ~88 KB/slot 地板
	const uint64_t minSsbo = floorTo(512ull * MSTRIDE, MSTRIDE);  // ~32 KB/slot 地板
	const uint64_t minInst = floorTo(1024ull * ISTRIDE, ISTRIDE);  // ~48 KB/slot 地板

	std::vector<uint64_t> vboSlice(numWorkers), ssboSlice(numWorkers), instSlice(numWorkers);
	auto splitWeighted = [&](uint64_t usable, uint64_t stride, uint64_t minBytes,
		const std::vector<uint32_t>& hist,
		std::vector<uint64_t>& out) {
			const bool haveHist = (hist.size() == (size_t)numWorkers);
			const uint64_t totalMin = (uint64_t)numWorkers * minBytes;
			// 地板都放不下 → 退回纯等分（极端兜底，实测 56MB vs ~1MB 地板不会触发）。
			if (!haveHist || totalMin >= usable) {
				const uint64_t eq = floorTo(usable / numWorkers, stride);
				for (int i = 0; i < numWorkers; ++i) out[i] = eq;
				return;
			}
			const uint64_t elastic = usable - totalMin;
			double sumW = 0.0;
			std::vector<double> w(numWorkers);
			for (int i = 0; i < numWorkers; ++i) { w[i] = std::max<double>(1.0, (double)hist[i]); sumW += w[i]; }
			for (int i = 0; i < numWorkers; ++i)
				out[i] = minBytes + floorTo((uint64_t)((double)elastic * (w[i] / sumW)), stride);
		};
	splitWeighted(vboUsable, VSTRIDE, minVbo, m_prevSliceVboDemand, vboSlice);
	splitWeighted(ssboUsable, MSTRIDE, minSsbo, m_prevSliceSsboDemand, ssboSlice);
	splitWeighted(instUsable, ISTRIDE, minInst, m_prevSliceInstDemand, instSlice);

	char* const vboBaseMap = static_cast<char*>(fr.vbo->MappedPtr()) + baseVbo;
	char* const ssboBaseMap = static_cast<char*>(fr.ssbo->MappedPtr()) + baseSsbo;
	char* const instBaseMap = static_cast<char*>(fr.instBuf->MappedPtr()) + baseInst;
	const uint32_t baseVertGlobal = (uint32_t)(baseVbo / sizeof(BatchVertex));
	const uint32_t baseMatGlobal = (uint32_t)(baseSsbo / sizeof(glm::mat4));
	const uint32_t baseInstGlobal = (uint32_t)(baseInst / sizeof(InstanceRecord));

	// 切片在区域内连续排布（前缀和偏移），无空洞；vbo / ssbo / inst 各自独立排布。
	uint64_t vOff = 0, mOff = 0, iOff = 0;
	for (int i = 0; i < numWorkers; ++i) {
		VkWorkerSlice& sl = m_workerRecords[i].slice;
		sl.vboPtr = reinterpret_cast<BatchVertex*>(vboBaseMap + vOff);
		sl.ssboPtr = reinterpret_cast<glm::mat4*>(ssboBaseMap + mOff);
		sl.instPtr = reinterpret_cast<InstanceRecord*>(instBaseMap + iOff);   // Task 3
		sl.vboBaseVert = baseVertGlobal + (uint32_t)(vOff / VSTRIDE);
		sl.ssboBaseMat = baseMatGlobal + (uint32_t)(mOff / MSTRIDE);
		sl.instBaseIdx = baseInstGlobal + (uint32_t)(iOff / ISTRIDE);              // Task 3
		sl.vboCap = (uint32_t)(vboSlice[i] / VSTRIDE);
		sl.ssboCap = (uint32_t)(ssboSlice[i] / MSTRIDE);
		sl.instCap = (uint32_t)(instSlice[i] / ISTRIDE);                       // Task 3
		// vboCount / ssboCount / instCount / overflowed 已经在上面 VkWorkerSlice{} 清零
		vOff += vboSlice[i];
		mOff += ssboSlice[i];
		iOff += instSlice[i];                                                       // Task 3
	}

	// 记录整段切片区域大小，ReplayAndEndParallel 收尾时一次性把游标推到末尾。
	m_parallelBaseVbo = baseVbo;
	m_parallelBaseSsbo = baseSsbo;
	m_parallelBaseInst = baseInst;
	m_parallelVboBytes = vOff;
	m_parallelSsboBytes = mOff;
	m_parallelInstBytes = iOff;
}

void Graphics::SetWorkerSlot(int slot) {
	if (slot < 0 || slot >= m_numActiveWorkers) {
		LOG_WARN("Graphics") << "SetWorkerSlot: invalid slot " << slot
			<< " (active=" << m_numActiveWorkers << ")";
		return;
	}
	WorkerRecord& r = m_workerRecords[slot];
	WorkerThreadState& s = m_workerStates[slot];

	// 用快照初始化 thread-local 变换栈 / 裁剪栈 / 混合模式。clear() 保留 capacity。
	s.transformStack.clear();
	s.transformIsIdentity.clear();
	s.clipStack.clear();
	s.transformStack.push_back(r.initialTopTransform);
	s.transformIsIdentity.push_back(r.initialTopIsIdentity ? 1 : 0);
	s.clipStack = r.initialClipStack;

	tl_record = &r;
	tl_transformStack = &s.transformStack;
	tl_transformIsIdentity = &s.transformIsIdentity;
	tl_clipStack = &s.clipStack;
	tl_blend = r.initialBlend;
}

void Graphics::ClearWorkerSlot() {
	// 防御性检查：worker 块结束时栈应平衡（GameObjectManager 总是把 Push/PopClip 配
	// 对调用；component 的 Push/PopTransform 也配对）。不平衡只打印警告，不阻塞，
	// 回放时主线程会自己保证 m_clipStack 起止一致。
	if (tl_record && tl_clipStack &&
		tl_clipStack->size() != tl_record->initialClipStack.size()) {
		LOG_WARN("Graphics") << "ClearWorkerSlot: clip stack imbalanced ("
			<< tl_clipStack->size() << " vs initial "
			<< tl_record->initialClipStack.size() << ")";
	}
	if (tl_transformStack && tl_transformStack->size() != 1) {
		LOG_WARN("Graphics") << "ClearWorkerSlot: transform stack imbalanced ("
			<< tl_transformStack->size() << ", expected 1)";
	}

	tl_record = nullptr;
	tl_transformStack = nullptr;
	tl_transformIsIdentity = nullptr;
	tl_clipStack = nullptr;
	tl_blend = BlendMode::None;
}

void Graphics::ReplayAndEndParallel() {
	// Phase 5（cmd-based blend）：worker 已经把顶点和矩阵直写进每帧 mapped VBO/SSBO 切片，
	// r.cmds 里只剩状态变更命令（PushClip / PopClip / SetBlend / DeferredText）。
	//
	// 关键：blend 分段走 cmd 流而非扫 per-vertex blendMode 字段。
	// 原因：mapped host-coherent 内存读取比 cache 内存慢 ~100×（write-combined），4400 僵尸
	// 场景下 ~26k quad × 一次读取 ≈ 几 ms 浪费在 CPU 主线程扫 blendMode 上，导致 Phase 5
	// 比 Phase 4 还慢。worker 已经在 SetBlendMode 切换时记了 SetBlend cmd，回放直接顺着 cmd
	// 跟踪当前 blend 即可，零 mapped 内存读。
	//
	// 副作用：pipeline 切换严格按 worker 写入顺序，而非按"实际 vert 的 blendMode 字段"。
	// 二者本质等价，因为 RecordDrawTexture 写 per-vert blendMode 时就是读 tl_blend——
	// SetBlend cmd 必定先于受影响的 vert 出现。

	if (m_numActiveWorkers == 0) return;
	if (!m_vk || !m_vk->frameOpen) {
		m_numActiveWorkers = 0;
		return;
	}
	VkCommandBuffer cb = m_vk->renderer->CurrentCmdBuffer();
	if (cb == VK_NULL_HANDLE) {
		m_numActiveWorkers = 0;
		return;
	}

	auto& fr = m_vk->frames[m_vk->frameIdx];

	// VBO 整段切片只 bind 一次。pipeline / descriptor / push-constant 只在 blend 真正变化
	// 时重绑（首次绑定也算变化）。
	VkBuffer vbuf = fr.vbo->Handle();
	VkDeviceSize vboOff = 0;
	vkCmdBindVertexBuffers(cb, 0, 1, &vbuf, &vboOff);

	const glm::mat4 projView = m_projection * m_viewMatrix;
	const VkDescriptorSet sets[2] = { fr.matrixSet, m_vk->texPool->DescriptorSet() };

	// 跨 slot 复用的 pipeline binding 状态：用一个"还没绑过"哨兵值，第一次 emit 时强制绑。
	enum class BoundPipe : uint8_t { None, BatchAlpha, BatchAdd, InstAlpha, InstAdd };
	BoundPipe boundPipe = BoundPipe::None;

	auto bindBatchForBlend = [&](BlendMode bm) {
		const BoundPipe want = (bm == BlendMode::Add) ? BoundPipe::BatchAdd : BoundPipe::BatchAlpha;
		if (want == boundPipe) return;
		pvz::VulkanPipeline* pipe = (want == BoundPipe::BatchAdd) ? m_vk->pipeBatchAdd.get()
			: m_vk->pipeBatchAlpha.get();
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Layout(),
			0, 2, sets, 0, nullptr);
		vkCmdPushConstants(cb, pipe->Layout(), VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(glm::mat4), &projView);
		boundPipe = want;
		};

	// 6.2: Instance pipeline bind helper — uses fr.instSet at set=0 (not fr.matrixSet).
	auto bindInstForBlend = [&](BlendMode bm) {
		const BoundPipe want = (bm == BlendMode::Add) ? BoundPipe::InstAdd : BoundPipe::InstAlpha;
		if (want == boundPipe) return;
		pvz::VulkanPipeline* pipe = (want == BoundPipe::InstAdd) ? m_vk->pipeInstAdd.get()
			: m_vk->pipeInstAlpha.get();
		const VkDescriptorSet instSets[2] = { fr.instSet, m_vk->texPool->DescriptorSet() };
		vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Handle());
		vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe->Layout(),
			0, 2, instSets, 0, nullptr);
		vkCmdPushConstants(cb, pipe->Layout(), VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(glm::mat4), &projView);
		boundPipe = want;
		};

	// DeferredText 渲染策略（按 cmd 的 onTop 标志区分）：
	//   onTop=false → 在该 cmd 记录的几何位置就地交错渲染（与对象同 z-order，如血量显示）；
	//   onTop=true  → 收集到此，待所有 slot 几何 emit 完后统一渲染（绝对顶层）。
	std::vector<const DeferredTextCmd*> pendingTopText;

	// 把帧游标提前推到 post-parallel 预留区：下面 onTop=false 的内联渲染会走 FlushBatch 写顶点，
	// 必须落在 BeginParallelRecord 保留的余量内，不能与各 slot 切片数据撞车。
	// （原本在 slot 循环之后才推；内联渲染要求提前。其后的 overlay 串行绘制会从这里继续累加。）
	fr.vboCursor  = (VkDeviceSize)(m_parallelBaseVbo  + m_parallelVboBytes);
	fr.ssboCursor = (VkDeviceSize)(m_parallelBaseSsbo + m_parallelSsboBytes);
	fr.instCursor = (VkDeviceSize)(m_parallelBaseInst + m_parallelInstBytes);

	for (int slot = 0; slot < m_numActiveWorkers; ++slot) {
		WorkerRecord& r = m_workerRecords[slot];
		VkWorkerSlice& sl = r.slice;

		if (sl.overflowed && !fr.overflowWarned) {
			LOG_WARN("Graphics") << "Worker slot " << slot
				<< " overflowed slice (vbo " << sl.vboCount << "/" << sl.vboCap
				<< ", ssbo " << sl.ssboCount << "/" << sl.ssboCap
				<< ", inst " << sl.instCount << "/" << sl.instCap << ")";
			fr.overflowWarned = true;
		}

		// 本 slot 起始 blend 来自 BeginParallelRecord 抓取的快照，replay 主线程也同步影子状态。
		BlendMode curBlend = r.initialBlend;
		m_currentBlendMode = curBlend;

		uint32_t drawStart = sl.vboBaseVert;
		uint32_t instStart = sl.instBaseIdx;

		// Emit batch verts AND instance records up to the given absolute cutoffs.
		// Order: batch first, then instance — matches PvZ z-order convention
		// (shadow → reanim) within a single BlendMode segment in a slot.
		auto emitUpTo = [&](uint32_t absVbo, uint32_t absInst) {
			if (absVbo > drawStart) {
				bindBatchForBlend(curBlend);
				vkCmdDraw(cb, absVbo - drawStart, 1, drawStart, 0);
				drawStart = absVbo;
			}
			if (absInst > instStart) {
				bindInstForBlend(curBlend);
				// 6 vertices per instance, instanceCount = (absInst - instStart),
				// firstInstance = instStart (absolute index into fr.instBuf).
				vkCmdDraw(cb, 6, absInst - instStart, 0, instStart);
				instStart = absInst;
			}
			};

		for (const RecordCmd& cmd : r.cmds) {
			const uint32_t cmdAbsVert = sl.vboBaseVert + cmd.vertOffsetAtCmd;
			const uint32_t cmdAbsInst = sl.instBaseIdx + cmd.instOffsetAtCmd;
			// 命令插入点之前的顶点 + 实例必须先 emit（用 cmd 应用 *之前* 的状态）
			emitUpTo(cmdAbsVert, cmdAbsInst);

			switch (cmd.type) {
			case RecCmdType::PushClip: {
				const ClipRect& cr = r.clips[cmd.payloadIdx];
				// PushClipRect 内部会 FlushBatch（此时 m_batchVertices 为空，no-op），
				// 然后压栈 + vkCmdSetScissor。
				PushClipRect(cr.x, cr.y, cr.w, cr.h);
				break;
			}
			case RecCmdType::PopClip: {
				PopClipRect();
				break;
			}
			case RecCmdType::SetBlend: {
				curBlend = r.blendModes[cmd.payloadIdx];
				m_currentBlendMode = curBlend;
				// 不立刻 bind pipeline——等下一次 emitUpTo 真要画的时候再绑，
				// 避免连续 SetBlend 之间空跑 vkCmdBindPipeline。
				break;
			}
			case RecCmdType::DeferredText: {
				const DeferredTextCmd& t = r.textCmds[cmd.payloadIdx];
				if (t.onTop) {
					// 绝对顶层：留到所有几何 emit 完后统一画。
					pendingTopText.push_back(&t);
				}
				else {
					// 当前层：emitUpTo 已把本对象 sprite（记录在该 cmd 之前）emit 完，此处就地画文字，
					// 文字便夹在"本对象 sprite"与"后续对象"之间 = 与对象同 z-order。
					// DrawText/FlushBatch 会重绑 pipeline/descriptor/vbo，画完清空 boundPipe 哨兵，
					// 强制下一次 emitUpTo 重新绑定，避免后续几何沿用文字管线。
					DrawText(t.text, t.fontKey, t.fontSize, t.color, t.x, t.y, t.scale);
					FlushBatch();
					boundPipe = BoundPipe::None;
				}
				break;
			}
			}
		}

		// slot 末尾：剩余顶点 + 剩余实例 emit
		emitUpTo(sl.vboBaseVert + sl.vboCount, sl.instBaseIdx + sl.instCount);
	}

	// 自适应负载均衡：采样本帧各 slot 的实际占用，作为下一并行帧的切片权重。
	// 溢出 slot 的真实需求已被丢弃无法测量——记为 cap×1.5 放大估计，使其下帧
	// 拿到更大切片（快 attack，1~2 帧内收敛）；非溢出 slot 直接用真实 count。
	// 必须在 m_numActiveWorkers 清零前采，且 slice 计数此刻已最终化。
	if ((int)m_prevSliceVboDemand.size() != m_numActiveWorkers) m_prevSliceVboDemand.assign(m_numActiveWorkers, 0);
	if ((int)m_prevSliceSsboDemand.size() != m_numActiveWorkers) m_prevSliceSsboDemand.assign(m_numActiveWorkers, 0);
	if ((int)m_prevSliceInstDemand.size() != m_numActiveWorkers) m_prevSliceInstDemand.assign(m_numActiveWorkers, 0);   // Task 3
	for (int slot = 0; slot < m_numActiveWorkers; ++slot) {
		const VkWorkerSlice& sl = m_workerRecords[slot].slice;
		m_prevSliceVboDemand[slot] = sl.overflowed ? (uint32_t)((uint64_t)sl.vboCap * 3u / 2u) : sl.vboCount;
		m_prevSliceSsboDemand[slot] = sl.overflowed ? (uint32_t)((uint64_t)sl.ssboCap * 3u / 2u) : sl.ssboCount;
		m_prevSliceInstDemand[slot] = sl.overflowed ? (uint32_t)((uint64_t)sl.instCap * 3u / 2u) : sl.instCount;       // Task 3
	}

	// onTop=true 的 deferred text 在所有 slot 几何之后统一渲染 = 绝对顶层。
	// （帧游标已在 slot 循环前推到 post-parallel 预留区，onTop=false 的内联文字也已从该区累加写入；
	//   后续 overlay 串行绘制继续从当前游标累加。）
	if (!pendingTopText.empty()) {
		for (const DeferredTextCmd* t : pendingTopText) {
			DrawText(t->text, t->fontKey, t->fontSize, t->color, t->x, t->y, t->scale);
		}
		FlushBatch();
	}

	m_numActiveWorkers = 0;
	// 不释放 record 存储；下帧 Reset 复用 capacity。
}

// ----------------------------------------------------------------------------
//  Phase 5：Record 路径直接把 BatchVertex / glm::mat4 写进 r.slice 指向的当前帧
//  mapped VBO/SSBO 切片。texIndex 直接是 bindless 槽位绝对值；matrixIndex 直接是
//  (slice.ssboBaseMat + slice.ssboCount)，即整帧 SSBO 中的绝对槽位。回放完全不再
//  做索引重映射。
//
//  公共写顶点的"6-vert quad / 1 mat4"模板在下面 EmitQuad 里抽出来；非 quad 的
//  RecordFillCircle 直接展开。
// ----------------------------------------------------------------------------

namespace {
	// 容量检查：6 顶点 + 1 mat4。返回 false 表示溢出（slot 标记 overflowed，调用方早退）。
	inline bool SliceHasRoom(VkWorkerSlice& sl, uint32_t addVerts, uint32_t addMats) {
		if (sl.vboCount + addVerts > sl.vboCap || sl.ssboCount + addMats > sl.ssboCap) {
			sl.overflowed = true;
			return false;
		}
		return true;
	}

	// 把一个矩阵推进切片 SSBO，返回它在整帧 SSBO 中的绝对下标。
	// 调用方必须先 SliceHasRoom 通过。
	inline uint32_t PushSliceMatrix(VkWorkerSlice& sl, const glm::mat4& m) {
		const uint32_t abs = sl.ssboBaseMat + sl.ssboCount;
		sl.ssboPtr[sl.ssboCount++] = m;
		return abs;
	}

	// 写一个 6-vert quad 到切片。pos/uv 按 (x,y) 矩形铺。texIdx / matAbs 是绝对槽位。
	// quad 排布与公开 DrawTexture（Graphics.cpp 中 BatchVertex vertices[6]）保持一致。
	inline void EmitQuad(VkWorkerSlice& sl,
		float u0, float v0, float u1, float v1,
		uint32_t texIdx, uint32_t matAbs,
		float r, float g, float b, float a, float bm)
	{
		BatchVertex* v = sl.vboPtr + sl.vboCount;
		v[0] = { 0.0f, 1.0f, u0, v1, texIdx, matAbs, r, g, b, a, bm };
		v[1] = { 1.0f, 1.0f, u1, v1, texIdx, matAbs, r, g, b, a, bm };
		v[2] = { 1.0f, 0.0f, u1, v0, texIdx, matAbs, r, g, b, a, bm };
		v[3] = { 0.0f, 1.0f, u0, v1, texIdx, matAbs, r, g, b, a, bm };
		v[4] = { 0.0f, 0.0f, u0, v0, texIdx, matAbs, r, g, b, a, bm };
		v[5] = { 1.0f, 0.0f, u1, v0, texIdx, matAbs, r, g, b, a, bm };
		sl.vboCount += 6;
	}

	// 把一条"已经在世界坐标里的"端点拼成 1 px 宽的世界坐标 quad，写进切片。
	// 与公开 DrawLine / DrawRect / DrawCircle 的 1px-wide quad 走法保持一致。
	// 调用方负责传 matAbs（通常对应 worker 端的单位矩阵）。
	inline void EmitEdgeQuad(VkWorkerSlice& sl,
		float ax, float ay, float bx, float by,
		uint32_t texIdx, uint32_t matAbs,
		float rr, float gg, float bb, float aa, float bm)
	{
		const float edx = bx - ax, edy = by - ay;
		const float len = std::sqrt(edx * edx + edy * edy);
		if (len < 0.001f) return;
		const float nx = -edy / len * 0.5f, ny = edx / len * 0.5f;

		BatchVertex* v = sl.vboPtr + sl.vboCount;
		v[0] = { ax + nx, ay + ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		v[1] = { ax - nx, ay - ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		v[2] = { bx - nx, by - ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		v[3] = { ax + nx, ay + ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		v[4] = { bx - nx, by - ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		v[5] = { bx + nx, by + ny, 0.5f, 0.5f, texIdx, matAbs, rr, gg, bb, aa, bm };
		sl.vboCount += 6;
	}
} // anonymous

void Graphics::RecordDrawTexture(WorkerRecord& r,
	const Texture* tex, float x, float y, float width, float height,
	float rotation, const glm::vec4& tint)
{
	if (!tex) return;
	VkWorkerSlice& sl = r.slice;
	if (!SliceHasRoom(sl, 6, 1)) return;

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	if (rotation != 0.0f) {
		local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
		local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
	}
	const bool curId = tl_transformIsIdentity->back() != 0;
	const glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const uint32_t matAbs = PushSliceMatrix(sl, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;
	EmitQuad(sl, 0.0f, 0.0f, 1.0f, 1.0f, (uint32_t)tex->id, matAbs, nt.r, nt.g, nt.b, nt.a, bm);
}

void Graphics::RecordDrawTextureMatrix(WorkerRecord& r,
	const Texture* tex, const glm::mat4& transform,
	float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode)
{
	// Animator 最热路径（~9 万次/帧）。镜像公开 DrawTextureMatrix 的批处理段，但写进切片。
	if (!tex) return;

	// 显式 blendMode 参数语义为"per-draw override，不污染全局状态"——与非录制路径
	// （DrawTextureMatrix 只把 actualMode 写进 per-vertex bm 字段而不动 m_currentBlendMode）
	// 对齐。回放按 SetBlend 命令分段切流水线，所以这里必须在画完后把 tl_blend 恢复回去，
	// 否则 Animator 画完发光 quad 留下的 Add 状态会污染本 slot 后续所有 DrawTexture 调用
	// （典型现象：下一个 GameObject 的 ShadowComponent 影子被 Add 混合成不可见）。
	const BlendMode savedBlend = tl_blend;
	const bool needSwitch = (blendMode != BlendMode::None && blendMode != tl_blend);
	if (needSwitch) {
		RecordSetBlendMode(r, blendMode);   // 插入 SetBlend 命令并更新 tl_blend
	}

	VkWorkerSlice& sl = r.slice;
	if (!SliceHasRoom(sl, 6, 1)) {
		if (needSwitch) RecordSetBlendMode(r, savedBlend);   // 容量不足提前返回也要恢复
		return;
	}

	const Texture* bindTex = tex;
	float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
	if (tex->atlasPage) {
		bindTex = tex->atlasPage;
		u0 = tex->aU0; v0 = tex->aV0; u1 = tex->aU1; v1 = tex->aV1;
	}

	glm::mat4 pivotTransform;
	if (pivotX != 0.0f || pivotY != 0.0f) {
		const glm::vec3 pivot(pivotX, pivotY, 0.0f);
		pivotTransform = glm::translate(glm::mat4(1.0f), pivot)
			* transform
			* glm::translate(glm::mat4(1.0f), -pivot);
	}
	else {
		pivotTransform = transform;
	}
	// 单位阵快速路径——Animator 通常压入单位阵作为基线，跳过 mat4×mat4 节省大量浮点。
	const bool curId = tl_transformIsIdentity->back() != 0;
	const glm::mat4 finalMatrix = curId ? pivotTransform
		: (tl_transformStack->back() * pivotTransform);
	const uint32_t matAbs = PushSliceMatrix(sl, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const BlendMode actualMode = (blendMode == BlendMode::None) ? tl_blend : blendMode;
	const float bm = (actualMode == BlendMode::Add) ? 1.0f : 0.0f;
	EmitQuad(sl, u0, v0, u1, v1, (uint32_t)bindTex->id, matAbs, nt.r, nt.g, nt.b, nt.a, bm);

	if (needSwitch) {
		RecordSetBlendMode(r, savedBlend);   // 恢复进入时的 tl_blend，避免污染后续绘制
	}
}

void Graphics::RecordDrawTextureRegion(WorkerRecord& r,
	const Texture* tex,
	float srcX, float srcY, float srcW, float srcH,
	float dstX, float dstY, float dstW, float dstH,
	float rotation, const glm::vec4& tint)
{
	if (!tex || tex->id == 0) return;
	VkWorkerSlice& sl = r.slice;
	if (!SliceHasRoom(sl, 6, 1)) return;

	const float u0 = srcX / tex->width;
	const float v0 = srcY / tex->height;
	const float u1 = (srcX + srcW) / tex->width;
	const float v1 = (srcY + srcH) / tex->height;

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(dstX, dstY, 0.0f));
	local = glm::scale(local, glm::vec3(dstW, dstH, 1.0f));
	if (rotation != 0.0f) {
		local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
		local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
	}
	const bool curId = tl_transformIsIdentity->back() != 0;
	const glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const uint32_t matAbs = PushSliceMatrix(sl, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;
	EmitQuad(sl, u0, v0, u1, v1, (uint32_t)tex->id, matAbs, nt.r, nt.g, nt.b, nt.a, bm);
}

void Graphics::RecordDrawCachedText(WorkerRecord& r,
	const CachedText& handle, float x, float y, float scale)
{
	// CachedText 是 immutable POD——worker 直接走快速路径，不需要 DeferredText。
	if (handle.textureID == 0) return;
	// 过期句柄丢弃（同 DrawCachedText）。m_textGeneration 只在帧外（RecomputeLetterbox）变更，
	// 并行录制期间稳定，worker 读取安全。
	if (handle.generation != m_textGeneration) return;
	VkWorkerSlice& sl = r.slice;
	if (!SliceHasRoom(sl, 6, 1)) return;

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	// 超采样纹理：除回逻辑尺寸保证屏幕布局不变（handle.superSample=1 时等价）。
	const float inv = scale / handle.superSample;
	local = glm::scale(local, glm::vec3(handle.width * inv, handle.height * inv, 1.0f));
	const bool curId = tl_transformIsIdentity->back() != 0;
	const glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const uint32_t matAbs = PushSliceMatrix(sl, finalMatrix);

	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;
	EmitQuad(sl, 0.0f, 0.0f, 1.0f, 1.0f, handle.textureID, matAbs, 1.0f, 1.0f, 1.0f, 1.0f, bm);
}

void Graphics::RecordDrawText(WorkerRecord& r,
	const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale, bool onTop)
{
	// DrawText 涉及 TTF 渲染 + GPU 上传 + LRU 维护，全部都不是线程安全的——一律 defer 到主线程。
	// onTop=false：ReplayAndEndParallel 在该 cmd 记录的几何位置就地渲染（对象同 z-order）。
	// onTop=true ：收集到所有 slot 几何之后统一渲染（绝对顶层）。
	DeferredTextCmd t;
	t.text = text;
	t.fontKey = fontKey;
	t.fontSize = fontSize;
	t.color = color;
	t.x = x;
	t.y = y;
	t.scale = scale;
	t.onTop = onTop;
	r.textCmds.push_back(std::move(t));

	RecordCmd c{};
	c.type = RecCmdType::DeferredText;
	c.vertOffsetAtCmd = r.slice.vboCount;
	c.instOffsetAtCmd = r.slice.instCount;   // Task 4
	c.payloadIdx = (uint32_t)(r.textCmds.size() - 1);
	r.cmds.push_back(c);
}

void Graphics::RecordFillRect(WorkerRecord& r,
	float x, float y, float width, float height, const glm::vec4& color)
{
	VkWorkerSlice& sl = r.slice;
	if (!SliceHasRoom(sl, 6, 1)) return;

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	const bool curId = tl_transformIsIdentity->back() != 0;
	const glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const uint32_t matAbs = PushSliceMatrix(sl, finalMatrix);

	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;
	EmitQuad(sl, 0.0f, 0.0f, 1.0f, 1.0f, m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm);
}

void Graphics::RecordDrawLine(WorkerRecord& r,
	float x1, float y1, float x2, float y2, const glm::vec4& color)
{
	VkWorkerSlice& sl = r.slice;
	// 单段线：6 顶点 + 1 单位矩阵
	if (!SliceHasRoom(sl, 6, 1)) return;

	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 p0 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
	const glm::vec4 p1 = transform * glm::vec4(x2, y2, 0.0f, 1.0f);

	// 顶点本身已经是世界坐标，所以矩阵走单位阵（shader 用它做 model 变换会等价 no-op）。
	const uint32_t matAbs = PushSliceMatrix(sl, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;
	EmitEdgeQuad(sl, p0.x, p0.y, p1.x, p1.y, m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm);
}

void Graphics::RecordDrawRect(WorkerRecord& r,
	float x, float y, float width, float height, const glm::vec4& color)
{
	VkWorkerSlice& sl = r.slice;
	// 矩形边：4 个边段，每段 6 顶点 = 24 顶点 + 1 共享单位矩阵
	if (!SliceHasRoom(sl, 24, 1)) return;

	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 p[4] = {
		transform * glm::vec4(x,         y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y + height, 0.0f, 1.0f),
		transform * glm::vec4(x,         y + height, 0.0f, 1.0f),
	};
	const uint32_t matAbs = PushSliceMatrix(sl, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < 4; ++i) {
		const int j = (i + 1) % 4;
		EmitEdgeQuad(sl, p[i].x, p[i].y, p[j].x, p[j].y,
			m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm);
	}
}

void Graphics::RecordDrawCircle(WorkerRecord& r,
	float cx, float cy, float radius, const glm::vec4& color, int segments)
{
	if (segments <= 0) return;
	VkWorkerSlice& sl = r.slice;
	// segments 个边段，每段 6 顶点 + 1 共享矩阵
	if (!SliceHasRoom(sl, (uint32_t)segments * 6, 1)) return;

	const glm::mat4& transform = tl_transformStack->back();
	const uint32_t matAbs = PushSliceMatrix(sl, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		const float a0 = 2.0f * glm::pi<float>() * i / segments;
		const float a1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		const glm::vec4 p0 = transform * glm::vec4(cx + radius * std::cos(a0), cy + radius * std::sin(a0), 0.0f, 1.0f);
		const glm::vec4 p1 = transform * glm::vec4(cx + radius * std::cos(a1), cy + radius * std::sin(a1), 0.0f, 1.0f);
		EmitEdgeQuad(sl, p0.x, p0.y, p1.x, p1.y,
			m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm);
	}
}

void Graphics::RecordFillCircle(WorkerRecord& r,
	float cx, float cy, float radius, const glm::vec4& color, int segments)
{
	if (segments <= 0) return;
	VkWorkerSlice& sl = r.slice;
	// 三角扇：segments 个三角形 × 3 顶点 + 1 共享矩阵。
	// 注意 EmitDrawRange 是 6 顶点 stride，3 顶点会被对齐丢弃——但 FillCircle 几乎只在
	// 调试路径（-Debug 显示 hitbox）使用，且原 RecordFillCircle 也是 3 顶点/段，对齐
	// 行为与原实现一致（不会有人在并行 record 路径里画大量 FillCircle）。
	if (!SliceHasRoom(sl, (uint32_t)segments * 3, 1)) return;

	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 center = transform * glm::vec4(cx, cy, 0.0f, 1.0f);
	const uint32_t matAbs = PushSliceMatrix(sl, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		const float a0 = 2.0f * glm::pi<float>() * i / segments;
		const float a1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		const glm::vec4 p0 = transform * glm::vec4(cx + radius * std::cos(a0), cy + radius * std::sin(a0), 0.0f, 1.0f);
		const glm::vec4 p1 = transform * glm::vec4(cx + radius * std::cos(a1), cy + radius * std::sin(a1), 0.0f, 1.0f);

		BatchVertex* v = sl.vboPtr + sl.vboCount;
		v[0] = { center.x, center.y, 0.5f, 0.5f, m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm };
		v[1] = { p0.x,     p0.y,     0.5f, 0.5f, m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm };
		v[2] = { p1.x,     p1.y,     0.5f, 0.5f, m_whiteTexture, matAbs, nc.r, nc.g, nc.b, nc.a, bm };
		sl.vboCount += 3;
	}
}

void Graphics::RecordPushClipRect(WorkerRecord& r, int x, int y, int w, int h) {
	// 在 worker 本地 clipStack 上做嵌套交集，replay 时再调真实 PushClipRect 触发 vkCmdSetScissor。
	// 本地维护是为了让 worker 内后续的 Push 能正确计算交集。
	ClipRect rect = { x, y, w, h };
	if (!tl_clipStack->empty()) {
		rect = IntersectClip(tl_clipStack->back(), rect);
	}
	tl_clipStack->push_back(rect);

	r.clips.push_back(rect);
	RecordCmd c{};
	c.type = RecCmdType::PushClip;
	c.vertOffsetAtCmd = r.slice.vboCount;
	c.instOffsetAtCmd = r.slice.instCount;   // Task 4
	c.payloadIdx = (uint32_t)(r.clips.size() - 1);
	r.cmds.push_back(c);
}

void Graphics::RecordPopClipRect(WorkerRecord& r) {
	if (tl_clipStack->empty()) {
		LOG_WARN("Graphics") << "RecordPopClipRect: worker clip stack underflow";
		return;
	}
	tl_clipStack->pop_back();

	RecordCmd c{};
	c.type = RecCmdType::PopClip;
	c.vertOffsetAtCmd = r.slice.vboCount;
	c.instOffsetAtCmd = r.slice.instCount;   // Task 4
	c.payloadIdx = 0;
	r.cmds.push_back(c);
}

void Graphics::RecordSetBlendMode(WorkerRecord& r, BlendMode mode) {
	if (mode == tl_blend) return;
	tl_blend = mode;

	r.blendModes.push_back(mode);
	RecordCmd c{};
	c.type = RecCmdType::SetBlend;
	c.vertOffsetAtCmd = r.slice.vboCount;
	c.instOffsetAtCmd = r.slice.instCount;   // Task 4
	c.payloadIdx = (uint32_t)(r.blendModes.size() - 1);
	r.cmds.push_back(c);
}