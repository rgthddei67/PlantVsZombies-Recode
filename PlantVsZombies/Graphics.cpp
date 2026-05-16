#include "Graphics.h"
#include "Profiler.h"

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
	thread_local WorkerRecord*           tl_record              = nullptr;
	thread_local std::vector<glm::mat4>* tl_transformStack      = nullptr;
	thread_local std::vector<char>*      tl_transformIsIdentity = nullptr;
	thread_local std::vector<ClipRect>*  tl_clipStack           = nullptr;
	thread_local BlendMode               tl_blend               = BlendMode::None;

	// 在 worker 本地 texList 中查找/插入 textureID，返回索引。texList 通常 <= 64，
	// 线性扫描足够快。
	inline int InternTex(WorkerRecord& r, GLuint textureID) {
		const size_t n = r.texList.size();
		for (size_t i = 0; i < n; ++i) {
			if (r.texList[i] == textureID) return (int)i;
		}
		r.texList.push_back(textureID);
		return (int)n;
	}

	// 把矩阵 push 到本地 matList，返回索引。我们不去重——Animator 几乎不会重复使
	// 用同一矩阵，hash mat4 的成本会盖过收益。
	inline int InternMat(WorkerRecord& r, const glm::mat4& m) {
		const int idx = (int)r.matList.size();
		r.matList.push_back(m);
		return idx;
	}
}

Graphics::Graphics() {
	m_transformStack.push_back(glm::mat4(1.0f));
	m_transformIsIdentity.push_back(1);
	this->SetBlendMode(BlendMode::Alpha);
}

Graphics::~Graphics() {
	// Phase 3a：GL 资源句柄全部未分配（Init 不再申请），析构只清 CPU 侧的批处理 vector + 文字缓存。
	FlushBatch();
	ClearTextCache();
	ClearPinnedTextCache();
}

bool Graphics::Initialize(int windowWidth, int windowHeight) {
	// Phase 3a stub：不再创建任何 GL 资源。
	// 给批处理上限设保守默认值，让 worker record 的 reserve()、AddMatrix 的 flush 阈值
	// 等纯 CPU 逻辑继续按"原 GL 上限"工作；后续 phase 切到 Vulkan 后再用实际硬件能力填充。
	m_maxTextureUnits  = 32;
	m_matrixBatchLimit = 1024;
	m_vertexBatchLimit = m_matrixBatchLimit * 6;
	m_useSSBO          = false;
	m_whiteTexture     = 0;  // batch 路径里通过 BindTexture(0) 走得通——只占一个 slot 不真采样

	SetWindowSize(windowWidth, windowHeight);
	return true;
}

bool Graphics::InitShaders() {
	// Phase 3a stub：不再加载 GLSL/编译 program。Phase 3b/3c 用 VulkanPipeline 取代。
	return true;
}

bool Graphics::InitSpriteRenderer() {
	// Phase 3a stub：立即模式 sprite 路径已废弃（plan 中标记 sprite_*.glsl 删除）。
	return true;
}

void Graphics::SetWindowSize(int width, int height) {
	m_windowWidth = width;
	m_windowHeight = height;
	m_projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
	// Phase 3a：视口由 Vulkan dynamic state 处理，CPU 侧只更新投影矩阵。
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
			std::cerr << "[Graphics] PopTransform (worker) failed: stack underflow." << std::endl;
		}
		return;
	}
	if (m_transformStack.size() > 1) {
		m_transformStack.pop_back();
		m_transformIsIdentity.pop_back();
	}
	else {
		std::cerr << "[Graphics] PopTransform failed: stack underflow." << std::endl;
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
	// Phase 3a stub：clip 栈在 CPU 侧维护；scissor 实际应用在 Phase 3c 接到 vkCmdSetScissor。
}

void Graphics::PushClipRect(int x, int y, int w, int h) {
	if (tl_record) { RecordPushClipRect(*tl_record, x, y, w, h); return; }
	// 必须先把已积累的顶点刷出去，否则 push 之前 batched 的内容会被新的 scissor 一起裁剪。
	FlushBatch();
	FlushGeomBatch();

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
		std::cerr << "[Graphics] PopClipRect failed: stack underflow." << std::endl;
		return;
	}
	// 在切换 scissor 状态之前先 flush，把当前 rect 下的顶点全部提交。
	FlushBatch();
	FlushGeomBatch();
	m_clipStack.pop_back();
	if (!m_clipStack.empty()) {
		ApplyTopClipRectToGL();
	}
	// Phase 3a：栈空时无需特殊处理（ApplyTopClipRectToGL 本身已是 no-op）。
}

void Graphics::BindVAO(GLuint vao) {
	// Phase 3a stub：VAO 概念在 Vulkan 中由 pipeline + bound buffer 取代。
	m_currentVAO = vao;
}

void Graphics::BeginBatch() {
	// 自动累积，无需操作
}

void Graphics::EndBatch() {
	FlushBatch();
}

void Graphics::FlushBatch() {
	// Phase 3a stub：GL 上传 + draw 全部去掉，仅清 CPU 缓冲。CheckBatch 的 flush 阈值
	// 因此变成"周期性回收 vector 容量"，不影响正确性。Phase 3b 在这里接 Vulkan 路径。
	m_batchVertices.clear();
	m_batchTextures.clear();
	m_batchMatrices.clear();
	FlushGeomBatch();
}

void Graphics::SetBatchMode(bool enable) {
	if (m_batchMode != enable) {
		FlushBatch();   // 切换模式前提交当前批次
		m_batchMode = enable;
	}
}

int Graphics::BindTexture(GLuint textureID) {
	// 查找是否已存在
	for (size_t i = 0; i < m_batchTextures.size(); ++i) {
		if (m_batchTextures[i] == textureID)
			return (int)i;
	}
	// 未存在，且纹理单元未满，则添加
	if ((int)m_batchTextures.size() < m_maxTextureUnits) {
		m_batchTextures.push_back(textureID);
		return (int)(m_batchTextures.size() - 1);
	}
	else {
		// 纹理单元已满，强制刷新并重新开始
		FlushBatch();
		m_batchTextures.push_back(textureID);
		return 0;
	}
}

int Graphics::AddMatrix(const glm::mat4& matrix) {
	if ((int)m_batchMatrices.size() >= m_matrixBatchLimit) {
		FlushBatch();
	}
	m_batchMatrices.push_back(matrix);
	return (int)(m_batchMatrices.size() - 1);
}

void Graphics::AddVertices(const BatchVertex* vertices, int count) {
	for (int i = 0; i < count; ++i) {
		m_batchVertices.push_back(vertices[i]);
	}
}

void Graphics::CheckBatch() {
	if ((int)m_batchVertices.size() >= m_vertexBatchLimit ||
		(int)m_batchMatrices.size() >= m_matrixBatchLimit ||
		(int)m_batchTextures.size() >= m_maxTextureUnits) {
		FlushBatch();
	}
}

void Graphics::DrawTexture(const GLTexture* tex, float x, float y, float width, float height,
	float rotation, const glm::vec4& tint) {
	if (!tex) {
		std::cerr << "[Graphics] Texture not found: " << tex << std::endl;
		return;
	}
	if (tl_record) { RecordDrawTexture(*tl_record, tex, x, y, width, height, rotation, tint); return; }

	if (m_batchMode) {
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
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b , normalizedTint.a, bm},
			{1.0f, 1.0f, 1.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g , normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b , normalizedTint.a, bm},
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r , normalizedTint.g, normalizedTint.b , normalizedTint.a, bm},
			{0.0f, 0.0f, 0.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r , normalizedTint.g , normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b , normalizedTint.a, bm}
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	// Phase 3a：m_batchMode 始终为 true，立即模式分支已删（依赖 sprite shader）。
}

void Graphics::DrawTextureMatrix(const GLTexture* tex, const glm::mat4& transform,
	float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode) {

	if (!tex) return;
	if (tl_record) { RecordDrawTextureMatrix(*tl_record, tex, transform, pivotX, pivotY, tint, blendMode); return; }

	if (m_batchMode) {
		// 图集重映射：若该纹理已被打进图集页，则改绑图集页并把 UV 收窄到子矩形
		const GLTexture* bindTex = tex;
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
			{0.0f, 1.0f, u0, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 1.0f, u1, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, u1, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{0.0f, 1.0f, u0, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{0.0f, 0.0f, u0, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, u1, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm}
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	// Phase 3a：立即模式分支已删。
}

void Graphics::DrawTextureRegion(const GLTexture* tex,
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

	if (m_batchMode) {
		int texIndex = BindTexture(tex->id);
		glm::mat4 finalMatrix = m_transformStack.back() * local;
		int matrixIndex = AddMatrix(finalMatrix);

		// 转换颜色为 0-1 格式
		glm::vec4 normalizedTint = NormalizeColor(tint);

		float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;
		BatchVertex vertices[6] = {
			{0.0f, 1.0f, u0, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 1.0f, u1, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, u1, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{0.0f, 1.0f, u0, v1, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{0.0f, 0.0f, u0, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm},
			{1.0f, 0.0f, u1, v0, (GLuint)texIndex, (GLuint)matrixIndex, normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a, bm}
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	// Phase 3a：立即模式分支已删（local 仅 batch 分支用）。
}

GLuint Graphics::GetOrCreateTextTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color, int& outWidth, int& outHeight) {
	// 生成缓存键
	std::stringstream ss;
	ss << text << "|" << fontKey << "|" << fontSize << "|"
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
	if (!RenderTextToGLTexture(text, fontKey, fontSize, color, entry)) {
		return 0;
	}
	outWidth = entry.width;
	outHeight = entry.height;

	// 超出容量时淘汰最久未使用的条目
	if ((int)m_textCache.size() >= TEXT_CACHE_MAX_SIZE) {
		const std::string& lruKey = m_textCacheOrder.back();
		auto lruIt = m_textCache.find(lruKey);
		if (lruIt != m_textCache.end()) {
			// Phase 3a stub：textureID 都是 0；3c 时改回收 bindless 槽位。
			m_textCache.erase(lruIt);
		}
		m_textCacheOrder.pop_back();
	}

	// 插入新条目到链表头部
	m_textCacheOrder.push_front(key);
	m_textCache[key] = { entry, m_textCacheOrder.begin() };
	return entry.textureID;
}

bool Graphics::RenderTextToGLTexture(const std::string& /*text*/, const std::string& /*fontKey*/,
	int /*fontSize*/, const glm::vec4& /*color*/, CachedText& /*out*/) {
	// Phase 3a stub：返回 false 让上层（GetOrCreateTextTexture / AcquireTextTexture）放弃缓存写入，
	// DrawText / DrawCachedText 的 textureID == 0 检查随即触发 early-return，于是文字在 3a 期间不显示。
	// 3c 时这里会改成：SDL_Surface → 上传到 VulkanTexturePool，返回带 bindlessIndex 的句柄。
	return false;
}

CachedText Graphics::AcquireTextTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color) {
	std::stringstream ss;
	ss << text << "|" << fontKey << "|" << fontSize << "|"
		<< (int)color.r << "," << (int)color.g << "," << (int)color.b << "," << (int)color.a;
	std::string key = ss.str();

	auto it = m_pinnedTextCache.find(key);
	if (it != m_pinnedTextCache.end()) {
		return it->second;
	}

	// RenderTextToGLTexture 走 glGenTextures/glTexImage2D，且要写共享的
	// m_pinnedTextCache——两者都只能在持有 GL 上下文的主线程做。worker 线程
	// （tl_record 非空，例如并行 Draw 录制阶段）上未命中时直接放弃本帧返回空
	// 句柄，由主线程预热缓存后即稳定命中。缺少此守卫会在 worker 线程拿到无效/
	// 串号的纹理 ID（表现为数字变成贴图或别的文字），并发写 map 还会损坏容器。
	if (tl_record) return {};

	CachedText entry;
	if (!RenderTextToGLTexture(text, fontKey, fontSize, color, entry)) {
		return {};
	}
	m_pinnedTextCache.emplace(std::move(key), entry);
	return entry;
}

void Graphics::DrawCachedText(const CachedText& handle, float x, float y, float scale) {
	if (handle.textureID == 0) return;
	if (tl_record) { RecordDrawCachedText(*tl_record, handle, x, y, scale); return; }

	if (m_batchMode) {
		int texIndex = BindTexture(handle.textureID);
		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(handle.width * scale, handle.height * scale, 1.0f));
		glm::mat4 finalMatrix = m_transformStack.back() * local;
		int matrixIndex = AddMatrix(finalMatrix);

		BatchVertex vertices[6] = {
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 1.0f, 1.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{0.0f, 0.0f, 0.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f}
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	// Phase 3a：立即模式分支已删。
}

void Graphics::ClearPinnedTextCache() {
	// Phase 3a stub：纹理 ID 都是 0（RenderTextToGLTexture 不再上传），无需 glDeleteTextures。
	m_pinnedTextCache.clear();
}

void Graphics::DrawText(const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale) {
	if (tl_record) { RecordDrawText(*tl_record, text, fontKey, fontSize, color, x, y, scale); return; }

	int w, h;
	GLuint texID = GetOrCreateTextTexture(text, fontKey, fontSize, color, w, h);
	if (texID == 0) return;

	if (m_batchMode) {
		int texIndex = BindTexture(texID);
		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(w * scale, h * scale, 1.0f));
		glm::mat4 finalMatrix = m_transformStack.back() * local;
		int matrixIndex = AddMatrix(finalMatrix);

		BatchVertex vertices[6] = {
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 1.0f, 1.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{0.0f, 0.0f, 0.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, 1,1,1,1, 0.0f}
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	// Phase 3a：立即模式分支已删。
}

void Graphics::ClearTextCache() {
	// Phase 3a stub：同 ClearPinnedTextCache，无 GL 资源。
	m_textCache.clear();
	m_textCacheOrder.clear();
}

void Graphics::SetClearColor(float r, float g, float b, float a) {
	// Phase 3a：保存归一化到 [0,1]，每帧由 VulkanRenderer::DrawFrame 读取。
	m_clearR = r / 255.0f;
	m_clearG = g / 255.0f;
	m_clearB = b / 255.0f;
	m_clearA = a / 255.0f;
}

void Graphics::SetBlendMode(BlendMode mode) {
	if (tl_record) { RecordSetBlendMode(*tl_record, mode); return; }

	if (m_currentBlendMode == mode) return;

	// 几何批次无逐顶点混合模式，切换时需提交
	if (!m_geomBatchVertices.empty()) {
		FlushGeomBatch();
	}

	m_currentBlendMode = mode;

	// 纹理批次通过逐顶点 blendMode 字段在 FlushBatch 时分段处理，无需提交。
	// Phase 3a stub：GL 状态切换由 3b 在 Vulkan pipeline 切换处理。
}

void Graphics::Clear() {
	// Phase 3a：真正的清屏由 VulkanRenderer::DrawFrame 完成；这里只做 CPU 侧的卫生检查。
	if (!m_clipStack.empty()) {
		std::cerr << "[Graphics] Unbalanced PushClipRect/PopClipRect across frames ("
			<< m_clipStack.size() << " entries left); resetting." << std::endl;
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

glm::vec2 Graphics::ScreenToWorldPosition(float screenX, float screenY) const {
	// 屏幕坐标 → NDC（注意 Y 轴翻转，因为正交投影 top=0）
	float ndcX = (screenX / m_windowWidth) * 2.0f - 1.0f;
	float ndcY = 1.0f - (screenY / m_windowHeight) * 2.0f;
	glm::vec4 worldPos = glm::inverse(m_projection * m_viewMatrix) * glm::vec4(ndcX, ndcY, 0.0f, 1.0f);
	return glm::vec2(worldPos.x, worldPos.y);
}

glm::vec2 Graphics::WorldToScreenPosition(float worldX, float worldY) const {
	glm::vec4 clipPos = m_projection * m_viewMatrix * glm::vec4(worldX, worldY, 0.0f, 1.0f);
	// NDC → 屏幕坐标（Y 轴翻转）
	float screenX = (clipPos.x + 1.0f) * 0.5f * m_windowWidth;
	float screenY = (1.0f - clipPos.y) * 0.5f * m_windowHeight;
	return glm::vec2(screenX, screenY);
}

void Graphics::FlushGeomBatch() {
	// Phase 3a stub：仅清 CPU 缓冲。
	m_geomBatchVertices.clear();
}

void Graphics::ResizeBatchBuffer(size_t newCapacity) {
	// Phase 3a stub：不再有 GL VBO；只更新记账值，便于 3b 重新挂 Vulkan VBO。
	m_batchBufferCapacity = newCapacity;
}

bool Graphics::InitGeomRenderer() {
	// Phase 3a stub。3c 时接 pipeGeomTris / pipeGeomLines。
	m_geomBatchCapacity = GEOM_BATCH_LIMIT;
	return true;
}

void Graphics::DrawGeomImmediate(const std::vector<GeomVertex>& vertices, GLenum mode) {
	if (vertices.empty()) return;

	// Phase 3a：始终走 batch 路径（m_batchMode 不会为 false），下面的 if 是历史结构保留。
	if (m_batchMode) {
		// 统一为两种可合并的图元类型
		GLenum batchMode = (mode == GL_TRIANGLES || mode == GL_TRIANGLE_FAN) ? GL_TRIANGLES : GL_LINES;

		// 图元类型切换时先刷新
		if (!m_geomBatchVertices.empty() && m_geomBatchMode != batchMode) {
			FlushGeomBatch();
		}
		m_geomBatchMode = batchMode;

		const glm::mat4& transform = m_transformStack.back();

		if (mode == GL_LINE_LOOP) {
			// 展开为 GL_LINES（每条边两个顶点）
			for (size_t i = 0; i < vertices.size(); ++i) {
				const auto& v0 = vertices[i];
				const auto& v1 = vertices[(i + 1) % vertices.size()];
				glm::vec4 p0 = transform * glm::vec4(v0.x, v0.y, 0.0f, 1.0f);
				glm::vec4 p1 = transform * glm::vec4(v1.x, v1.y, 0.0f, 1.0f);
				m_geomBatchVertices.push_back({ p0.x, p0.y, v0.r, v0.g, v0.b, v0.a });
				m_geomBatchVertices.push_back({ p1.x, p1.y, v1.r, v1.g, v1.b, v1.a });
			}
		}
		else if (mode == GL_TRIANGLE_FAN) {
			// 展开为 GL_TRIANGLES
			for (size_t i = 1; i + 1 < vertices.size(); ++i) {
				const auto& v0 = vertices[0];
				const auto& vi = vertices[i];
				const auto& vi1 = vertices[i + 1];
				glm::vec4 p0 = transform * glm::vec4(v0.x, v0.y, 0.0f, 1.0f);
				glm::vec4 pi = transform * glm::vec4(vi.x, vi.y, 0.0f, 1.0f);
				glm::vec4 pi1 = transform * glm::vec4(vi1.x, vi1.y, 0.0f, 1.0f);
				m_geomBatchVertices.push_back({ p0.x,  p0.y,  v0.r,  v0.g,  v0.b,  v0.a });
				m_geomBatchVertices.push_back({ pi.x,  pi.y,  vi.r,  vi.g,  vi.b,  vi.a });
				m_geomBatchVertices.push_back({ pi1.x, pi1.y, vi1.r, vi1.g, vi1.b, vi1.a });
			}
		}
		else {
			// GL_LINES / GL_TRIANGLES：直接加入，顶点乘以当前变换
			for (const auto& v : vertices) {
				glm::vec4 p = transform * glm::vec4(v.x, v.y, 0.0f, 1.0f);
				m_geomBatchVertices.push_back({ p.x, p.y, v.r, v.g, v.b, v.a });
			}
		}

		if ((int)m_geomBatchVertices.size() >= GEOM_BATCH_LIMIT) {
			FlushGeomBatch();
		}
		return;
	}
	// Phase 3a：立即模式分支已删。
}

void Graphics::DrawLine(float x1, float y1, float x2, float y2, const glm::vec4& color) {
	if (tl_record) { RecordDrawLine(*tl_record, x1, y1, x2, y2, color); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	if (m_batchMode) {
		// batch 模式：将线段转为 1px 宽四边形，走纹理批次保证绘制顺序
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
			{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			{p0.x - nx, p0.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			{p1.x + nx, p1.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
		};
		AddVertices(verts, 6);
		CheckBatch();
	}
	else {
		std::vector<GeomVertex> verts = {
			{x1, y1, r, g, b, a},
			{x2, y2, r, g, b, a}
		};
		DrawGeomImmediate(verts, GL_LINES);
	}
}

void Graphics::DrawRect(float x, float y, float width, float height, const glm::vec4& color) {
	if (tl_record) { RecordDrawRect(*tl_record, x, y, width, height, color); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	if (m_batchMode) {
		// batch 模式：4条边各转为 1px 宽四边形，走纹理批次
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
				{ax + nx, ay + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{ax - nx, ay - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{bx - nx, by - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{ax + nx, ay + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{bx - nx, by - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{bx + nx, by + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			};
			AddVertices(verts, 6);
		}
		CheckBatch();
	}
	else {
		std::vector<GeomVertex> verts = {
			{x,         y,          r, g, b, a},
			{x + width, y,          r, g, b, a},
			{x + width, y + height, r, g, b, a},
			{x,         y + height, r, g, b, a}
		};
		DrawGeomImmediate(verts, GL_LINE_LOOP);
	}
}

void Graphics::FillRect(float x, float y, float width, float height, const glm::vec4& color) {
	if (tl_record) { RecordFillRect(*tl_record, x, y, width, height, color); return; }

	if (m_batchMode) {
		// 批处理模式：使用 1×1 白色纹理加入纹理批次，保证与其他纹理的绘制顺序正确
		int texIndex = BindTexture(m_whiteTexture);

		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(width, height, 1.0f));
		glm::mat4 finalMatrix = m_transformStack.back() * local;
		int matrixIndex = AddMatrix(finalMatrix);

		glm::vec4 nc = NormalizeColor(color);
		float bm = (m_currentBlendMode == BlendMode::Add) ? 1.0f : 0.0f;
		BatchVertex vertices[6] = {
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
			{1.0f, 1.0f, 1.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
			{0.0f, 1.0f, 0.0f, 1.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
			{0.0f, 0.0f, 0.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
			{1.0f, 0.0f, 1.0f, 0.0f, (GLuint)texIndex, (GLuint)matrixIndex, nc.r, nc.g, nc.b, nc.a, bm},
		};
		AddVertices(vertices, 6);
		CheckBatch();
	}
	else {
		glm::vec4 nc = NormalizeColor(color);
		float r = nc.r, g = nc.g, b = nc.b, a = nc.a;
		std::vector<GeomVertex> verts = {
			{x,         y,          r, g, b, a},
			{x + width, y,          r, g, b, a},
			{x + width, y + height, r, g, b, a},
			{x,         y,          r, g, b, a},
			{x + width, y + height, r, g, b, a},
			{x,         y + height, r, g, b, a}
		};
		DrawGeomImmediate(verts, GL_TRIANGLES);
	}
}

void Graphics::DrawCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	if (tl_record) { RecordDrawCircle(*tl_record, cx, cy, radius, color, segments); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	if (m_batchMode) {
		// batch 模式：每段弧转为 1px 宽四边形，走纹理批次
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
				{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p0.x - nx, p0.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p0.x + nx, p0.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p1.x - nx, p1.y - ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p1.x + nx, p1.y + ny, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			};
			AddVertices(verts, 6);
		}
		CheckBatch();
	}
	else {
		std::vector<GeomVertex> verts;
		verts.reserve(segments);
		for (int i = 0; i < segments; ++i) {
			float angle = 2.0f * glm::pi<float>() * i / segments;
			verts.push_back({ cx + radius * cosf(angle), cy + radius * sinf(angle), r, g, b, a });
		}
		DrawGeomImmediate(verts, GL_LINE_LOOP);
	}
}

void Graphics::FillCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	if (tl_record) { RecordFillCircle(*tl_record, cx, cy, radius, color, segments); return; }

	glm::vec4 nc = NormalizeColor(color);
	float r = nc.r, g = nc.g, b = nc.b, a = nc.a;

	if (m_batchMode) {
		// batch 模式：展开三角扇，走纹理批次
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
				{center.x, center.y, 0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p0.x, p0.y,         0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
				{p1.x, p1.y,         0.5f, 0.5f, (GLuint)texIndex, (GLuint)matIndex, r,g,b,a, bm},
			};
			AddVertices(verts, 3);
		}
		CheckBatch();
	}
	else {
		std::vector<GeomVertex> verts;
		verts.reserve(segments + 2);
		verts.push_back({ cx, cy, r, g, b, a });
		for (int i = 0; i <= segments; ++i) {
			float angle = 2.0f * glm::pi<float>() * i / segments;
			verts.push_back({ cx + radius * cosf(angle), cy + radius * sinf(angle), r, g, b, a });
		}
		DrawGeomImmediate(verts, GL_TRIANGLE_FAN);
	}
}

void Graphics::Submit(std::function<void()> cmd) {
	if (tl_record) {
		// worker 内调用 Submit：录到本 slot 的 customCmds，回放时按顺序在主线程执行
		tl_record->customCmds.push_back(std::move(cmd));
		RecordCmd c{};
		c.type       = static_cast<uint32_t>(RecCmdType::Custom);
		c.payloadIdx = static_cast<uint32_t>(tl_record->customCmds.size() - 1);
		tl_record->cmds.push_back(c);
		return;
	}
	std::lock_guard<std::mutex> lock(m_commandMutex);
	m_pendingCommands.push_back(std::move(cmd));
}

void Graphics::ProcessCommandQueue() {
	{
		std::lock_guard<std::mutex> lock(m_commandMutex);
		std::swap(m_pendingCommands, m_activeCommands);
	}
	for (auto& cmd : m_activeCommands) {
		cmd();
	}
	m_activeCommands.clear();
}

void Graphics::SubmitDrawTexture(const GLTexture* texture, float x, float y, float width, float height,
	float rotation, const glm::vec4& tint) {
	Submit([=]() { DrawTexture(texture, x, y, width, height, rotation, tint); });
}

void Graphics::SubmitDrawTextureMatrix(const GLTexture* texture, const glm::mat4& transform,
	float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode) {
	Submit([=]() { DrawTextureMatrix(texture, transform, pivotX, pivotY, tint, blendMode); });
}

void Graphics::SubmitDrawTextureRegion(const GLTexture* tex,
	float srcX, float srcY, float srcW, float srcH,
	float dstX, float dstY, float dstW, float dstH,
	float rotation, const glm::vec4& tint)
{
	Submit([=]() { DrawTextureRegion(tex, srcX, srcY, srcW, srcH, dstX, dstY, dstW, dstH, rotation, tint); });
}

void Graphics::SubmitDrawText(const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale) {
	Submit([=]() { DrawText(text, fontKey, fontSize, color, x, y, scale); });
}

void Graphics::SubmitDrawLine(float x1, float y1, float x2, float y2, const glm::vec4& color) {
	Submit([=]() { DrawLine(x1, y1, x2, y2, color); });
}

void Graphics::SubmitDrawRect(float x, float y, float width, float height, const glm::vec4& color) {
	Submit([=]() { DrawRect(x, y, width, height, color); });
}

void Graphics::SubmitFillRect(float x, float y, float width, float height, const glm::vec4& color) {
	Submit([=]() { FillRect(x, y, width, height, color); });
}

void Graphics::SubmitDrawCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	Submit([=]() { DrawCircle(cx, cy, radius, color, segments); });
}

void Graphics::SubmitFillCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
	Submit([=]() { FillCircle(cx, cy, radius, color, segments); });
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

	// 确保 records 与 states 都有 numWorkers 份
	if ((int)m_workerRecords.size() < numWorkers) m_workerRecords.resize(numWorkers);
	if ((int)m_workerStates.size()  < numWorkers) m_workerStates.resize(numWorkers);

	// 当前 Graphics 状态作为每个 worker 的初始基线
	const glm::mat4& curTop = m_transformStack.back();
	const bool       curId  = m_transformIsIdentity.back() != 0;

	for (int i = 0; i < numWorkers; ++i) {
		WorkerRecord& r = m_workerRecords[i];
		r.Reset();

		// 首次使用此 slot 时按推荐容量 reserve，避免运行中 realloc 抖动。
		// 用 cmds.capacity() 作为"是否首帧"的廉价判定（Reset 不改 capacity）。
		if (r.cmds.capacity() == 0) {
			r.verts.reserve(96 * 1024);
			r.matList.reserve(16 * 1024);
			r.cmds.reserve(16 * 1024);
			r.texList.reserve(64);
			r.clips.reserve(8);
			r.blendModes.reserve(16);
			r.textCmds.reserve(64);
		}

		r.initialTopTransform  = curTop;
		r.initialTopIsIdentity = curId;
		r.initialClipStack     = m_clipStack;       // 拷贝当前裁剪栈
		r.initialBlend         = m_currentBlendMode;
	}

	m_numActiveWorkers = numWorkers;
}

void Graphics::SetWorkerSlot(int slot) {
	if (slot < 0 || slot >= m_numActiveWorkers) {
		std::cerr << "[Graphics] SetWorkerSlot: invalid slot " << slot
			<< " (active=" << m_numActiveWorkers << ")" << std::endl;
		return;
	}
	WorkerRecord&      r = m_workerRecords[slot];
	WorkerThreadState& s = m_workerStates[slot];

	// 用快照初始化 thread-local 变换栈 / 裁剪栈 / 混合模式。clear() 保留 capacity。
	s.transformStack.clear();
	s.transformIsIdentity.clear();
	s.clipStack.clear();
	s.transformStack.push_back(r.initialTopTransform);
	s.transformIsIdentity.push_back(r.initialTopIsIdentity ? 1 : 0);
	s.clipStack = r.initialClipStack;

	tl_record              = &r;
	tl_transformStack      = &s.transformStack;
	tl_transformIsIdentity = &s.transformIsIdentity;
	tl_clipStack           = &s.clipStack;
	tl_blend               = r.initialBlend;
}

void Graphics::ClearWorkerSlot() {
	// 防御性检查：worker 块结束时栈应平衡（GameObjectManager 总是把 Push/PopClip 配
	// 对调用；component 的 Push/PopTransform 也配对）。不平衡只打印警告，不阻塞，
	// 回放时主线程会自己保证 m_clipStack 起止一致。
	if (tl_record && tl_clipStack &&
		tl_clipStack->size() != tl_record->initialClipStack.size()) {
		std::cerr << "[Graphics] ClearWorkerSlot: clip stack imbalanced ("
			<< tl_clipStack->size() << " vs initial "
			<< tl_record->initialClipStack.size() << ")" << std::endl;
	}
	if (tl_transformStack && tl_transformStack->size() != 1) {
		std::cerr << "[Graphics] ClearWorkerSlot: transform stack imbalanced ("
			<< tl_transformStack->size() << ", expected 1)" << std::endl;
	}

	tl_record              = nullptr;
	tl_transformStack      = nullptr;
	tl_transformIsIdentity = nullptr;
	tl_clipStack           = nullptr;
	tl_blend               = BlendMode::None;
}

void Graphics::ReplayAndEndParallel() {
	// 优化点：
	// 1) **texMap 缓存**：worker 本地 texList 已经去重（典型 <=64 个），主线程不必每 cmd
	//    都跑一遍 BindTexture 的 32 元素线性扫描。给每个 slot 一份 texMap[localIdx]→
	//    globalSlot，命中即 O(1)；FlushBatch 一旦触发就全表失效。AddMatrix 触发的 flush
	//    会清空 m_batchTextures，所以也得重绑当前用到的纹理。
	//
	// 2) **resize + 单循环代替 insert + 第二轮 overwrite**：避免对同一段内存遍历两遍。
	//
	// 矩阵不缓存——worker 几乎不复用同一矩阵（每个 sprite 各算各的 finalMatrix），
	// hash mat4 的成本会盖过收益。
	std::vector<int> texMap;

	auto invalidateTexMap = [&]() { std::fill(texMap.begin(), texMap.end(), -1); };

	for (int slot = 0; slot < m_numActiveWorkers; ++slot) {
		WorkerRecord& r = m_workerRecords[slot];
		texMap.assign(r.texList.size(), -1);

		for (const RecordCmd& cmd : r.cmds) {
			switch (static_cast<RecCmdType>(cmd.type)) {

			case RecCmdType::BatchVerts: {
				const GLuint textureID = r.texList[cmd.localTexIdx];

				// 步骤 1：拿全局纹理槽位。先查 texMap 缓存；未命中则调 BindTexture。
				int gTex = texMap[cmd.localTexIdx];
				if (gTex < 0) {
					const size_t sz0 = m_batchVertices.size();
					gTex = BindTexture(textureID);
					if (m_batchVertices.size() < sz0) {
						// BindTexture 触发 FlushBatch（纹理槽位满）：旧缓存全部失效
						invalidateTexMap();
					}
					texMap[cmd.localTexIdx] = gTex;
				}

				// 步骤 2：拿全局矩阵槽位。AddMatrix 触发 flush 会同时清空 m_batchTextures，
				// 让上一步缓存的 gTex 失效——这种情况下要重绑纹理到新批次。
				const size_t sz1 = m_batchVertices.size();
				const int gMat = AddMatrix(r.matList[cmd.localMatIdx]);
				if (m_batchVertices.size() < sz1) {
					invalidateTexMap();
					gTex = BindTexture(textureID);
					texMap[cmd.localTexIdx] = gTex;
				}

				// 步骤 3：拷贝顶点 + 覆写两个索引字段。
				// 用 insert（trivially-copyable 类型内部走 memcpy/memmove，不会对新位置零初始化）
				// 而不是 resize+循环——resize 对聚合类型 BatchVertex 做 value-initialization
				// 会先全员清零再被我们整个覆写，每帧多写 31MB 零字节，反而拖累 cache。
				const size_t before = m_batchVertices.size();
				m_batchVertices.insert(m_batchVertices.end(),
					r.verts.begin() + cmd.vertOffset,
					r.verts.begin() + cmd.vertOffset + cmd.count);
				const GLuint gTexU = static_cast<GLuint>(gTex);
				const GLuint gMatU = static_cast<GLuint>(gMat);
				for (size_t i = before; i < before + cmd.count; ++i) {
					m_batchVertices[i].texIndex    = gTexU;
					m_batchVertices[i].matrixIndex = gMatU;
				}

				// 步骤 4：CheckBatch 可能触发 flush（顶点 / 矩阵 / 纹理满）；失效缓存。
				const size_t sz2 = m_batchVertices.size();
				CheckBatch();
				if (m_batchVertices.size() < sz2) {
					invalidateTexMap();
				}
				break;
			}

			case RecCmdType::PushClip: {
				const ClipRect& cr = r.clips[cmd.payloadIdx];
				PushClipRect(cr.x, cr.y, cr.w, cr.h);   // 内部 FlushBatch + glScissor
				invalidateTexMap();
				break;
			}

			case RecCmdType::PopClip: {
				PopClipRect();                          // 内部 FlushBatch
				invalidateTexMap();
				break;
			}

			case RecCmdType::SetBlend: {
				SetBlendMode(r.blendModes[cmd.payloadIdx]);
				// SetBlendMode 在批处理模式下不 flush 纹理批次，但保守失效一次代价极低
				break;
			}

			case RecCmdType::DeferredText: {
				const DeferredTextCmd& t = r.textCmds[cmd.payloadIdx];
				DrawText(t.text, t.fontKey, t.fontSize, t.color, t.x, t.y, t.scale);
				// DrawText 内部走批处理；保守失效缓存
				invalidateTexMap();
				break;
			}

			case RecCmdType::Custom: {
				r.customCmds[cmd.payloadIdx]();
				invalidateTexMap();
				break;
			}

			default:
				break;
			}
		}
	}

	m_numActiveWorkers = 0;
	// 不释放 record 存储；下帧 Reset 复用 capacity。
}

// ----------------------------------------------------------------------------
//  Record 路径辅助函数：与对应的公开 DrawXxx 批处理路径一一镜像，把矩阵乘法、UV
//  计算、顶点构造留在 worker 上跑；BindTexture 与 AddMatrix 的全局槽位分配延迟到
//  Replay 阶段（顶点的 texIndex/matrixIndex 字段先留 0xFFFFFFFF 占位）。
// ----------------------------------------------------------------------------

// 顶点用的占位值；Replay 会覆写。
static constexpr GLuint kPlaceholderIdx = 0xFFFFFFFFu;

void Graphics::RecordDrawTexture(WorkerRecord& r,
	const GLTexture* tex, float x, float y, float width, float height,
	float rotation, const glm::vec4& tint)
{
	if (!tex) return;
	const int localTex = InternTex(r, tex->id);

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	if (rotation != 0.0f) {
		local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
		local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
		local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
	}
	const bool curId = tl_transformIsIdentity->back() != 0;
	glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const int localMat = InternMat(r, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[1] = { 1.0f, 1.0f, 1.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[2] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[3] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[4] = { 0.0f, 0.0f, 0.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[5] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

void Graphics::RecordDrawTextureMatrix(WorkerRecord& r,
	const GLTexture* tex, const glm::mat4& transform,
	float pivotX, float pivotY, const glm::vec4& tint, BlendMode blendMode)
{
	// 这是 Animator 的最热路径（~9 万次/帧）。镜像 Graphics.cpp:547-594 的批处理段。
	if (!tex) return;

	const GLTexture* bindTex = tex;
	float u0 = 0.0f, v0 = 0.0f, u1 = 1.0f, v1 = 1.0f;
	if (tex->atlasPage) {
		bindTex = tex->atlasPage;
		u0 = tex->aU0; v0 = tex->aV0; u1 = tex->aU1; v1 = tex->aV1;
	}
	const int localTex = InternTex(r, bindTex->id);

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
	glm::mat4 finalMatrix = curId ? pivotTransform
		: (tl_transformStack->back() * pivotTransform);
	const int localMat = InternMat(r, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const BlendMode actualMode = (blendMode == BlendMode::None) ? tl_blend : blendMode;
	const float bm = (actualMode == BlendMode::Add) ? 1.0f : 0.0f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { 0.0f, 1.0f, u0, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[1] = { 1.0f, 1.0f, u1, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[2] = { 1.0f, 0.0f, u1, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[3] = { 0.0f, 1.0f, u0, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[4] = { 0.0f, 0.0f, u0, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[5] = { 1.0f, 0.0f, u1, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

void Graphics::RecordDrawTextureRegion(WorkerRecord& r,
	const GLTexture* tex,
	float srcX, float srcY, float srcW, float srcH,
	float dstX, float dstY, float dstW, float dstH,
	float rotation, const glm::vec4& tint)
{
	if (!tex || tex->id == 0) return;

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
	glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);

	const int localTex = InternTex(r, tex->id);
	const int localMat = InternMat(r, finalMatrix);

	const glm::vec4 nt = NormalizeColor(tint);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { 0.0f, 1.0f, u0, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[1] = { 1.0f, 1.0f, u1, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[2] = { 1.0f, 0.0f, u1, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[3] = { 0.0f, 1.0f, u0, v1, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[4] = { 0.0f, 0.0f, u0, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };
	v[5] = { 1.0f, 0.0f, u1, v0, kPlaceholderIdx, kPlaceholderIdx, nt.r, nt.g, nt.b, nt.a, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

void Graphics::RecordDrawCachedText(WorkerRecord& r,
	const CachedText& handle, float x, float y, float scale)
{
	// CachedText 是 immutable POD（textureID/width/height）——worker 直接走快速路径，
	// 不需要走 DeferredText。镜像 Graphics.cpp:824-844。
	if (handle.textureID == 0) return;
	const int localTex = InternTex(r, handle.textureID);

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(handle.width * scale, handle.height * scale, 1.0f));
	const bool curId = tl_transformIsIdentity->back() != 0;
	glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const int localMat = InternMat(r, finalMatrix);

	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };
	v[1] = { 1.0f, 1.0f, 1.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };
	v[2] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };
	v[3] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };
	v[4] = { 0.0f, 0.0f, 0.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };
	v[5] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, 1,1,1,1, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

void Graphics::RecordDrawText(WorkerRecord& r,
	const std::string& text, const std::string& fontKey, int fontSize,
	const glm::vec4& color, float x, float y, float scale)
{
	// DrawText 涉及 TTF 渲染 + glTexImage2D + LRU 维护，全部都不是线程安全的——
	// 一律 defer 到主线程。频率不高（数千次/帧，且 LRU 命中率高），影响可忽略。
	DeferredTextCmd t;
	t.text     = text;
	t.fontKey  = fontKey;
	t.fontSize = fontSize;
	t.color    = color;
	t.x        = x;
	t.y        = y;
	t.scale    = scale;
	r.textCmds.push_back(std::move(t));

	RecordCmd c{};
	c.type       = static_cast<uint32_t>(RecCmdType::DeferredText);
	c.payloadIdx = static_cast<uint32_t>(r.textCmds.size() - 1);
	r.cmds.push_back(c);
}

void Graphics::RecordFillRect(WorkerRecord& r,
	float x, float y, float width, float height, const glm::vec4& color)
{
	const int localTex = InternTex(r, m_whiteTexture);

	glm::mat4 local = glm::mat4(1.0f);
	local = glm::translate(local, glm::vec3(x, y, 0.0f));
	local = glm::scale(local, glm::vec3(width, height, 1.0f));
	const bool curId = tl_transformIsIdentity->back() != 0;
	glm::mat4 finalMatrix = curId ? local : (tl_transformStack->back() * local);
	const int localMat = InternMat(r, finalMatrix);

	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
	v[1] = { 1.0f, 1.0f, 1.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
	v[2] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
	v[3] = { 0.0f, 1.0f, 0.0f, 1.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
	v[4] = { 0.0f, 0.0f, 0.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
	v[5] = { 1.0f, 0.0f, 1.0f, 0.0f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

// 公共辅助：把已变换到世界坐标的边端点拼成 1px 宽四边形（线段、矩形边、圆边复用）。
// localTex/localMat 指向 m_whiteTexture / 单位阵在 worker 本地表中的索引。
static inline void AppendEdgeQuad(WorkerRecord& r,
	float ax, float ay, float bx, float by,
	float rr, float gg, float bb, float aa, float bm,
	int localTex, int localMat)
{
	const float edx = bx - ax, edy = by - ay;
	const float len = std::sqrt(edx * edx + edy * edy);
	if (len < 0.001f) return;
	const float nx = -edy / len * 0.5f, ny = edx / len * 0.5f;

	const size_t vOff = r.verts.size();
	r.verts.resize(vOff + 6);
	BatchVertex* v = r.verts.data() + vOff;
	v[0] = { ax + nx, ay + ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };
	v[1] = { ax - nx, ay - ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };
	v[2] = { bx - nx, by - ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };
	v[3] = { ax + nx, ay + ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };
	v[4] = { bx - nx, by - ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };
	v[5] = { bx + nx, by + ny, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, rr,gg,bb,aa, bm };

	RecordCmd c{};
	c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
	c.count       = 6;
	c.vertOffset  = static_cast<uint32_t>(vOff);
	c.localTexIdx = static_cast<uint16_t>(localTex);
	c.localMatIdx = static_cast<uint16_t>(localMat);
	r.cmds.push_back(c);
}

void Graphics::RecordDrawLine(WorkerRecord& r,
	float x1, float y1, float x2, float y2, const glm::vec4& color)
{
	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 p0 = transform * glm::vec4(x1, y1, 0.0f, 1.0f);
	const glm::vec4 p1 = transform * glm::vec4(x2, y2, 0.0f, 1.0f);

	const int localTex = InternTex(r, m_whiteTexture);
	const int localMat = InternMat(r, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	AppendEdgeQuad(r, p0.x, p0.y, p1.x, p1.y, nc.r, nc.g, nc.b, nc.a, bm, localTex, localMat);
}

void Graphics::RecordDrawRect(WorkerRecord& r,
	float x, float y, float width, float height, const glm::vec4& color)
{
	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 p[4] = {
		transform * glm::vec4(x,         y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y,          0.0f, 1.0f),
		transform * glm::vec4(x + width, y + height, 0.0f, 1.0f),
		transform * glm::vec4(x,         y + height, 0.0f, 1.0f),
	};
	const int localTex = InternTex(r, m_whiteTexture);
	const int localMat = InternMat(r, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < 4; ++i) {
		const int j = (i + 1) % 4;
		AppendEdgeQuad(r, p[i].x, p[i].y, p[j].x, p[j].y, nc.r, nc.g, nc.b, nc.a, bm, localTex, localMat);
	}
}

void Graphics::RecordDrawCircle(WorkerRecord& r,
	float cx, float cy, float radius, const glm::vec4& color, int segments)
{
	const glm::mat4& transform = tl_transformStack->back();
	const int localTex = InternTex(r, m_whiteTexture);
	const int localMat = InternMat(r, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		const float a0 = 2.0f * glm::pi<float>() * i / segments;
		const float a1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		const glm::vec4 p0 = transform * glm::vec4(cx + radius * std::cos(a0), cy + radius * std::sin(a0), 0.0f, 1.0f);
		const glm::vec4 p1 = transform * glm::vec4(cx + radius * std::cos(a1), cy + radius * std::sin(a1), 0.0f, 1.0f);
		AppendEdgeQuad(r, p0.x, p0.y, p1.x, p1.y, nc.r, nc.g, nc.b, nc.a, bm, localTex, localMat);
	}
}

void Graphics::RecordFillCircle(WorkerRecord& r,
	float cx, float cy, float radius, const glm::vec4& color, int segments)
{
	const glm::mat4& transform = tl_transformStack->back();
	const glm::vec4 center = transform * glm::vec4(cx, cy, 0.0f, 1.0f);

	const int localTex = InternTex(r, m_whiteTexture);
	const int localMat = InternMat(r, glm::mat4(1.0f));
	const glm::vec4 nc = NormalizeColor(color);
	const float bm = (tl_blend == BlendMode::Add) ? 1.0f : 0.0f;

	for (int i = 0; i < segments; ++i) {
		const float a0 = 2.0f * glm::pi<float>() * i / segments;
		const float a1 = 2.0f * glm::pi<float>() * (i + 1) / segments;
		const glm::vec4 p0 = transform * glm::vec4(cx + radius * std::cos(a0), cy + radius * std::sin(a0), 0.0f, 1.0f);
		const glm::vec4 p1 = transform * glm::vec4(cx + radius * std::cos(a1), cy + radius * std::sin(a1), 0.0f, 1.0f);

		const size_t vOff = r.verts.size();
		r.verts.resize(vOff + 3);
		BatchVertex* v = r.verts.data() + vOff;
		v[0] = { center.x, center.y, 0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
		v[1] = { p0.x,     p0.y,     0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };
		v[2] = { p1.x,     p1.y,     0.5f, 0.5f, kPlaceholderIdx, kPlaceholderIdx, nc.r, nc.g, nc.b, nc.a, bm };

		RecordCmd c{};
		c.type        = static_cast<uint32_t>(RecCmdType::BatchVerts);
		c.count       = 3;
		c.vertOffset  = static_cast<uint32_t>(vOff);
		c.localTexIdx = static_cast<uint16_t>(localTex);
		c.localMatIdx = static_cast<uint16_t>(localMat);
		r.cmds.push_back(c);
	}
}

void Graphics::RecordPushClipRect(WorkerRecord& r, int x, int y, int w, int h) {
	// 在 worker 本地 clipStack 上做嵌套交集，replay 时再调真实 PushClipRect 触发
	// FlushBatch + glScissor。本地维护是为了让 worker 内后续的 Push 能正确计算交集。
	ClipRect rect = { x, y, w, h };
	if (!tl_clipStack->empty()) {
		rect = IntersectClip(tl_clipStack->back(), rect);
	}
	tl_clipStack->push_back(rect);

	r.clips.push_back(rect);
	RecordCmd c{};
	c.type       = static_cast<uint32_t>(RecCmdType::PushClip);
	c.payloadIdx = static_cast<uint32_t>(r.clips.size() - 1);
	r.cmds.push_back(c);
}

void Graphics::RecordPopClipRect(WorkerRecord& r) {
	if (tl_clipStack->empty()) {
		std::cerr << "[Graphics] RecordPopClipRect: worker clip stack underflow" << std::endl;
		return;
	}
	tl_clipStack->pop_back();

	RecordCmd c{};
	c.type = static_cast<uint32_t>(RecCmdType::PopClip);
	r.cmds.push_back(c);
}

void Graphics::RecordSetBlendMode(WorkerRecord& r, BlendMode mode) {
	if (mode == tl_blend) return;
	tl_blend = mode;

	r.blendModes.push_back(mode);
	RecordCmd c{};
	c.type       = static_cast<uint32_t>(RecCmdType::SetBlend);
	c.payloadIdx = static_cast<uint32_t>(r.blendModes.size() - 1);
	r.cmds.push_back(c);
}