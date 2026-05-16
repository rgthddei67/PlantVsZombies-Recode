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
	FlushBatch();                       // 提交剩余顶点
	ClearTextCache();                    // 释放文字纹理
	ClearPinnedTextCache();              // 释放常驻文字纹理

	if (m_spriteVAO) glDeleteVertexArrays(1, &m_spriteVAO);
	if (m_spriteVBO) glDeleteBuffers(1, &m_spriteVBO);
	if (m_spriteEBO) glDeleteBuffers(1, &m_spriteEBO);

	if (m_batchVAO) glDeleteVertexArrays(1, &m_batchVAO);
	if (m_batchVBO) glDeleteBuffers(1, &m_batchVBO);
	if (m_matrixBuffer) glDeleteBuffers(1, &m_matrixBuffer);

	// 释放几何图形渲染资源
	if (m_geomVAO) glDeleteVertexArrays(1, &m_geomVAO);
	if (m_geomVBO) glDeleteBuffers(1, &m_geomVBO);

	if (m_whiteTexture) glDeleteTextures(1, &m_whiteTexture);
}

bool Graphics::Initialize(int windowWidth, int windowHeight) {
	// 查询最大纹理单元数，并限制不超过 32（着色器数组大小）
	glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_maxTextureUnits);
	if (m_maxTextureUnits > 32) m_maxTextureUnits = 32;

	// 查询 UBO 最大块大小，用它决定单批次矩阵上限（UBO 回退路径）
	// std140 保证 mat4 占 64 字节；GL 3.3 最低保证 GL_MAX_UNIFORM_BLOCK_SIZE >= 16KB（256 个 mat4），桌面 GPU 普遍为 64KB（1024 个）
	GLint maxUBOSize = 0;
	glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
	int computed = maxUBOSize > 0 ? maxUBOSize / static_cast<int>(sizeof(glm::mat4)) : MATRIX_BATCH_LIMIT_MIN;
	if (computed < MATRIX_BATCH_LIMIT_MIN) computed = MATRIX_BATCH_LIMIT_MIN;
	if (computed > 2048) computed = 2048;
	m_matrixBatchLimit = computed;

	// 检测 SSBO 支持（GL 4.3 core 或 GL_ARB_shader_storage_buffer_object 扩展）
	if (GLAD_GL_VERSION_4_3) {
		m_useSSBO = true;
	}
	else {
		GLint numExtensions = 0;
		glGetIntegerv(GL_NUM_EXTENSIONS, &numExtensions);
		for (GLint i = 0; i < numExtensions; ++i) {
			const char* ext = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, i));
			if (ext && std::string(ext) == "GL_ARB_shader_storage_buffer_object") {
				m_useSSBO = true;
				break;
			}
		}
	}
	if (m_useSSBO) {
		GLint maxSSBOSize = 0;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);
		if (maxSSBOSize > 0) {
			int ssboComputed = maxSSBOSize / static_cast<int>(sizeof(glm::mat4));
			if (ssboComputed > 262144) ssboComputed = 262144;
			if (ssboComputed > m_matrixBatchLimit) {
				m_matrixBatchLimit = ssboComputed;
			}
		}
		else {
			m_useSSBO = false;
		}
	}
	m_vertexBatchLimit = m_matrixBatchLimit * 6;

#ifdef _DEBUG
	std::cout << "[Graphics] GL_MAX_UNIFORM_BLOCK_SIZE=" << maxUBOSize
		<< "  SSBO=" << (m_useSSBO ? "yes" : "no")
		<< "  -> m_matrixBatchLimit=" << m_matrixBatchLimit
		<< "  m_vertexBatchLimit=" << m_vertexBatchLimit << std::endl;
#endif

	if (!InitShaders()) return false;
	if (!InitSpriteRenderer()) return false;
	if (!InitGeomRenderer()) return false;

	SetWindowSize(windowWidth, windowHeight);

	// 初始化批处理 VAO 和 VBO
	glGenVertexArrays(1, &m_batchVAO);
	glGenBuffers(1, &m_batchVBO);

	// 初始容量设为 m_vertexBatchLimit
	m_batchBufferCapacity = m_vertexBatchLimit;
	glBindVertexArray(m_batchVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);
	glBufferData(GL_ARRAY_BUFFER, m_batchBufferCapacity * sizeof(BatchVertex), nullptr, GL_DYNAMIC_DRAW);

	// 设置顶点属性指针（与 FlushBatch 中的布局一致）
	// 位置 (x,y)
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)0);
	glEnableVertexAttribArray(0);
	// 纹理坐标 (u,v)
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);
	// 纹理索引（整数属性，用 IPointer）
	glVertexAttribIPointer(2, 1, GL_UNSIGNED_INT, sizeof(BatchVertex), (void*)(4 * sizeof(float)));
	glEnableVertexAttribArray(2);
	// 矩阵索引（整数属性，用 IPointer）
	glVertexAttribIPointer(3, 1, GL_UNSIGNED_INT, sizeof(BatchVertex), (void*)(4 * sizeof(float) + sizeof(GLuint)));
	glEnableVertexAttribArray(3);
	// 颜色
	glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)(4 * sizeof(float) + 2 * sizeof(GLuint)));
	glEnableVertexAttribArray(4);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// 创建矩阵缓冲（SSBO 或 UBO），绑定到 binding point 0
	glGenBuffers(1, &m_matrixBuffer);
	if (m_useSSBO) {
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_matrixBuffer);
		glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(glm::mat4) * m_matrixBatchLimit, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_matrixBuffer);
	}
	else {
		glBindBuffer(GL_UNIFORM_BUFFER, m_matrixBuffer);
		glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * m_matrixBatchLimit, nullptr, GL_DYNAMIC_DRAW);
		glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_matrixBuffer);
	}

	// UBO 路径需要手动绑定 uniform block
	if (!m_useSSBO) {
		GLuint blockIndex = glGetUniformBlockIndex(m_batchShader.getProgramID(), "MatrixBlock");
		if (blockIndex == GL_INVALID_INDEX) {
			std::cerr << "[Graphics] MatrixBlock not found in batch shader (UBO binding failed)" << std::endl;
			return false;
		}
		glUniformBlockBinding(m_batchShader.getProgramID(), blockIndex, 0);
	}

	// 创建 1×1 纯白纹理，供 FillRect 批处理使用
	unsigned char whitePixel[4] = { 255, 255, 255, 255 };
	glGenTextures(1, &m_whiteTexture);
	glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, whitePixel);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glBindTexture(GL_TEXTURE_2D, 0);

	return true;
}

bool Graphics::InitShaders() {
	// 加载精灵着色器（立即模式）
	if (!m_spriteShader.loadFromFile("./Shader/sprite_vertex.glsl", "./Shader/sprite_fragment.glsl")) {
		std::cerr << "[Graphics] Failed to load sprite shader." << std::endl;
		return false;
	}

	// 加载批处理着色器；运行时把 MATRIX_BATCH_LIMIT 注入为 GLSL 宏，使数组维度与 C++ 端一致
	std::string batchDefines = "#define MATRIX_BATCH_LIMIT " + std::to_string(m_matrixBatchLimit) + "\n";
	if (m_useSSBO) {
		batchDefines = "#extension GL_ARB_shader_storage_buffer_object : require\n" + batchDefines;
		batchDefines += "#define USE_SSBO 1\n";
	}
	if (!m_batchShader.loadFromFile("./Shader/batch_vertex.glsl", "./Shader/batch_fragment.glsl", batchDefines)) {
		std::cerr << "[Graphics] Failed to load batch shader." << std::endl;
		return false;
	}

	// 设置批处理着色器的纹理采样器数组（绑定到纹理单元 0~31）
	m_batchShader.use();
	GLint texLoc = m_batchShader.getUniformLocation("textures");
	if (texLoc != -1) {
		int samplers[32];
		for (int i = 0; i < 32; ++i) samplers[i] = i;
		glUniform1iv(texLoc, 32, samplers);
	}
	return true;
}

bool Graphics::InitSpriteRenderer() {
	// 顶点数据：位置 + 纹理坐标（单位矩形）
	float vertices[] = {
		// 位置     // 纹理坐标
		0.0f, 1.0f, 0.0f, 1.0f,
		1.0f, 1.0f, 1.0f, 1.0f,
		1.0f, 0.0f, 1.0f, 0.0f,
		0.0f, 0.0f, 0.0f, 0.0f
	};
	unsigned int indices[] = { 0, 1, 2, 0, 2, 3 };

	glGenVertexArrays(1, &m_spriteVAO);
	glGenBuffers(1, &m_spriteVBO);
	glGenBuffers(1, &m_spriteEBO);

	glBindVertexArray(m_spriteVAO);

	glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_spriteEBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

	// 位置属性
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
	glEnableVertexAttribArray(0);
	// 纹理坐标属性
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

	glBindVertexArray(m_batchVAO);
	m_currentVAO = m_batchVAO;

	return true;
}

void Graphics::SetWindowSize(int width, int height) {
	m_windowWidth = width;
	m_windowHeight = height;
	m_projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
	glViewport(0, 0, width, height);
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
	const ClipRect& r = m_clipStack.back();
	int w = std::max(0, r.w);
	int h = std::max(0, r.h);
	// OpenGL scissor 用左下原点；窗口左上 (rx, ry) -> GL 左下 (rx, windowH - ry - h)
	int glY = m_windowHeight - r.y - h;
	glEnable(GL_SCISSOR_TEST);
	glScissor(r.x, glY, w, h);
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
	if (m_clipStack.empty()) {
		glDisable(GL_SCISSOR_TEST);
	}
	else {
		ApplyTopClipRectToGL();
	}
}

void Graphics::BindVAO(GLuint vao) {
	if (m_currentVAO != vao) {
		glBindVertexArray(vao);
		m_currentVAO = vao;
	}
}

void Graphics::BeginBatch() {
	// 自动累积，无需操作
}

void Graphics::EndBatch() {
	FlushBatch();
}

void Graphics::FlushBatch() {
	if (!m_batchVertices.empty()) {
		// Profiler::Get().CountFlush(m_batchVertices.size());
		// 确保 VBO 容量足够
		if (m_batchVertices.size() > m_batchBufferCapacity) {
			size_t newCapacity = m_batchVertices.size() * 2;
			ResizeBatchBuffer(newCapacity);
		}

		// 使用批处理着色器
		m_batchShader.use();

		// 上传投影矩阵
		glUniformMatrix4fv(m_batchShader.getUniformLocation("proj"), 1, GL_FALSE, glm::value_ptr(m_projection));

		// 上传所有变换矩阵到缓冲（SSBO 或 UBO）
		if (!m_batchMatrices.empty()) {
			GLenum target = m_useSSBO ? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER;
			glBindBuffer(target, m_matrixBuffer);
			glBufferSubData(target, 0,
				m_batchMatrices.size() * sizeof(glm::mat4),
				glm::value_ptr(m_batchMatrices[0]));
		}

		// 设置视图矩阵
		GLint viewLoc = m_batchShader.getUniformLocation("view");
		if (viewLoc != -1) {
			glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
		}

		// 绑定所有使用到的纹理到对应的纹理单元
		for (size_t i = 0; i < m_batchTextures.size(); ++i) {
			glActiveTexture(GL_TEXTURE0 + static_cast<int>(i));
			glBindTexture(GL_TEXTURE_2D, m_batchTextures[i]);
		}

		// 上传顶点数据到 VBO
		glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, m_batchVertices.size() * sizeof(BatchVertex), m_batchVertices.data());

		// 按连续相同 blendMode 分段绘制，每段设置对应 GL 混合状态
		BindVAO(m_batchVAO);
		size_t segStart = 0;
		while (segStart < m_batchVertices.size()) {
			float curBM = m_batchVertices[segStart].blendMode;
			size_t segEnd = segStart + 1;
			while (segEnd < m_batchVertices.size() && m_batchVertices[segEnd].blendMode == curBM) {
				++segEnd;
			}

			// 设置该段的混合模式
			if (curBM > 0.5f) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE);           // Additive
			}
			else {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // Alpha
			}

			glDrawArrays(GL_TRIANGLES, (GLint)segStart, (GLsizei)(segEnd - segStart));
			segStart = segEnd;
		}

		// 恢复当前混合模式对应的 GL 状态
		switch (m_currentBlendMode) {
		case BlendMode::Alpha:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			break;
		case BlendMode::Add:
			glBlendFunc(GL_SRC_ALPHA, GL_ONE);
			break;
		default:
			break;
		}

		// 清空批处理容器
		m_batchVertices.clear();
		m_batchTextures.clear();
		m_batchMatrices.clear();

		// 恢复默认纹理单元
		glActiveTexture(GL_TEXTURE0);
	}

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
	else {
		// 立即模式绘制
		glm::vec4 normalizedTint = NormalizeColor(tint);
		m_spriteShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->id);
		glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
		glUniform4f(m_spriteShader.getUniformLocation("color"), normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a);

		// 构建局部变换矩阵（同上）
		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(width, height, 1.0f));
		if (rotation != 0.0f) {
			local = glm::translate(local, glm::vec3(0.5f, 0.5f, 0.0f));
			local = glm::rotate(local, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
			local = glm::translate(local, glm::vec3(-0.5f, -0.5f, 0.0f));
		}
		glm::mat4 finalModel = m_transformStack.back() * local;
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

		BindVAO(m_spriteVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}
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
	else {
		// 立即模式
		glm::vec4 normalizedTint = NormalizeColor(tint);

		// 如果指定了混合模式且与当前不同，先切换
		BlendMode originalMode = m_currentBlendMode;
		if (blendMode != BlendMode::None && blendMode != m_currentBlendMode) {
			SetBlendMode(blendMode);
		}

		m_spriteShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->id);
		glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
		glUniform4f(m_spriteShader.getUniformLocation("color"), normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a);

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
		glm::mat4 finalModel = m_transformStack.back() * pivotTransform;
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

		BindVAO(m_spriteVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// 恢复原始混合模式
		if (m_currentBlendMode != originalMode) {
			SetBlendMode(originalMode);
		}
	}
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
	else {
		// 立即模式：用区域 UV 临时更新 sprite VBO
		float verts[] = {
			0.0f, 1.0f, u0, v1,
			1.0f, 1.0f, u1, v1,
			1.0f, 0.0f, u1, v0,
			0.0f, 0.0f, u0, v0
		};
		glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		glm::vec4 normalizedTint = NormalizeColor(tint);
		m_spriteShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, tex->id);
		glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
		glUniform4f(m_spriteShader.getUniformLocation("color"), normalizedTint.r, normalizedTint.g, normalizedTint.b, normalizedTint.a);

		glm::mat4 finalModel = m_transformStack.back() * local;
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

		BindVAO(m_spriteVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

		// 恢复全纹理 UV
		float fullVerts[] = {
			0.0f, 1.0f, 0.0f, 1.0f,
			1.0f, 1.0f, 1.0f, 1.0f,
			1.0f, 0.0f, 1.0f, 0.0f,
			0.0f, 0.0f, 0.0f, 0.0f
		};
		glBindBuffer(GL_ARRAY_BUFFER, m_spriteVBO);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(fullVerts), fullVerts);
		glBindBuffer(GL_ARRAY_BUFFER, 0);
	}
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
			glDeleteTextures(1, &lruIt->second.first.textureID);
			m_textCache.erase(lruIt);
		}
		m_textCacheOrder.pop_back();
	}

	// 插入新条目到链表头部
	m_textCacheOrder.push_front(key);
	m_textCache[key] = { entry, m_textCacheOrder.begin() };
	return entry.textureID;
}

bool Graphics::RenderTextToGLTexture(const std::string& text, const std::string& fontKey,
	int fontSize, const glm::vec4& color, CachedText& out) {
	TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
	if (!font) {
		std::cerr << "[RenderTextToGLTexture] Failed to get font: " << fontKey << " size " << fontSize << std::endl;
		return false;
	}

	SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), ToSDLColor(color));
	if (!surface) {
		std::cerr << "[RenderTextToGLTexture] TTF_RenderText_Blended error: " << TTF_GetError() << std::endl;
		return false;
	}

	SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
	SDL_FreeSurface(surface);
	if (!rgbaSurface) {
		std::cerr << "[RenderTextToGLTexture] Convert surface error: " << SDL_GetError() << std::endl;
		return false;
	}

	GLuint textureID;
	glGenTextures(1, &textureID);
	glBindTexture(GL_TEXTURE_2D, textureID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glPixelStorei(GL_UNPACK_ROW_LENGTH, rgbaSurface->pitch / 4);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, rgbaSurface->w, rgbaSurface->h, 0,
		GL_RGBA, GL_UNSIGNED_BYTE, rgbaSurface->pixels);
	glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

	out.textureID = textureID;
	out.width = rgbaSurface->w;
	out.height = rgbaSurface->h;
	SDL_FreeSurface(rgbaSurface);
	return true;
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
	else {
		m_spriteShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, handle.textureID);
		glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
		glUniform4f(m_spriteShader.getUniformLocation("color"), 1, 1, 1, 1);

		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(handle.width * scale, handle.height * scale, 1.0f));
		glm::mat4 finalModel = m_transformStack.back() * local;
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

		BindVAO(m_spriteVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}
}

void Graphics::ClearPinnedTextCache() {
	for (auto& pair : m_pinnedTextCache) {
		if (pair.second.textureID) {
			glDeleteTextures(1, &pair.second.textureID);
		}
	}
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
	else {
		// 立即模式
		m_spriteShader.use();
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, texID);
		glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
		glUniform4f(m_spriteShader.getUniformLocation("color"), 1, 1, 1, 1);

		glm::mat4 local = glm::mat4(1.0f);
		local = glm::translate(local, glm::vec3(x, y, 0.0f));
		local = glm::scale(local, glm::vec3(w * scale, h * scale, 1.0f));
		glm::mat4 finalModel = m_transformStack.back() * local;
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
		glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

		BindVAO(m_spriteVAO);
		glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}
}

void Graphics::ClearTextCache() {
	for (auto& pair : m_textCache) {
		if (pair.second.first.textureID) {
			glDeleteTextures(1, &pair.second.first.textureID);
		}
	}
	m_textCache.clear();
	m_textCacheOrder.clear();
}

void Graphics::SetClearColor(float r, float g, float b, float a) {
	glClearColor(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
}

void Graphics::SetBlendMode(BlendMode mode) {
	if (tl_record) { RecordSetBlendMode(*tl_record, mode); return; }

	if (m_currentBlendMode == mode) return;

	// 几何批次无逐顶点混合模式，切换时需提交
	if (!m_geomBatchVertices.empty()) {
		FlushGeomBatch();
	}

	m_currentBlendMode = mode;

	// 纹理批次通过逐顶点 blendMode 字段在 FlushBatch 时分段处理，无需提交
	// 非批处理模式下直接设置 GL 状态
	if (!m_batchMode) {
		if (!m_batchVertices.empty()) {
			FlushBatch();
		}
	}

	GLenum src, dst;
	switch (mode) {
	case BlendMode::Alpha:
		src = GL_SRC_ALPHA;
		dst = GL_ONE_MINUS_SRC_ALPHA;
		break;
	case BlendMode::Add:
		src = GL_SRC_ALPHA;
		dst = GL_ONE;
		break;
	default:
		return;
	}
	glEnable(GL_BLEND);
	glBlendFunc(src, dst);
}

void Graphics::Clear() {
	// 帧入口安全：scissor test 必须关闭，否则 glClear 只清空 scissor 区域内的像素，
	// 区域外会残留上一帧画面。同时检测裁剪栈是否上一帧没 pop 干净。
	if (!m_clipStack.empty()) {
		std::cerr << "[Graphics] Unbalanced PushClipRect/PopClipRect across frames ("
			<< m_clipStack.size() << " entries left); resetting." << std::endl;
		m_clipStack.clear();
	}
	glDisable(GL_SCISSOR_TEST);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
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
	if (m_geomBatchVertices.empty()) return;

	m_geomShader.use();
	glm::mat4 identity(1.0f);
	glUniformMatrix4fv(m_geomShader.getUniformLocation("proj"), 1, GL_FALSE, glm::value_ptr(m_projection));
	glUniformMatrix4fv(m_geomShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
	glUniformMatrix4fv(m_geomShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(identity));

	BindVAO(m_geomVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_geomVBO);

	// 按需扩容
	if (m_geomBatchVertices.size() > m_geomBatchCapacity) {
		m_geomBatchCapacity = m_geomBatchVertices.size() * 2;
		glBufferData(GL_ARRAY_BUFFER, m_geomBatchCapacity * sizeof(GeomVertex), nullptr, GL_DYNAMIC_DRAW);
	}

	glBufferSubData(GL_ARRAY_BUFFER, 0, m_geomBatchVertices.size() * sizeof(GeomVertex), m_geomBatchVertices.data());
	glDrawArrays(m_geomBatchMode, 0, (GLsizei)m_geomBatchVertices.size());

	m_geomBatchVertices.clear();
}

void Graphics::ResizeBatchBuffer(size_t newCapacity) {
	glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);
	glBufferData(GL_ARRAY_BUFFER, newCapacity * sizeof(BatchVertex), nullptr, GL_DYNAMIC_DRAW);
	m_batchBufferCapacity = newCapacity;
}

bool Graphics::InitGeomRenderer() {
	// 加载几何着色器（顶点+片段）
	if (!m_geomShader.loadFromFile("./Shader/geom_vertex.glsl", "./Shader/geom_fragment.glsl")) {
		std::cerr << "[Graphics] Failed to load geometry shader." << std::endl;
		return false;
	}

	glGenVertexArrays(1, &m_geomVAO);
	glGenBuffers(1, &m_geomVBO);

	glBindVertexArray(m_geomVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_geomVBO);

	// 预分配批处理缓冲区
	glBufferData(GL_ARRAY_BUFFER, GEOM_BATCH_LIMIT * sizeof(GeomVertex), nullptr, GL_DYNAMIC_DRAW);
	m_geomBatchCapacity = GEOM_BATCH_LIMIT;

	// 属性0：位置 (x, y)
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GeomVertex), (void*)0);
	glEnableVertexAttribArray(0);
	// 属性1：颜色 (r, g, b, a)
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(GeomVertex), (void*)(2 * sizeof(float)));
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	return true;
}

void Graphics::DrawGeomImmediate(const std::vector<GeomVertex>& vertices, GLenum mode) {
	if (vertices.empty()) return;

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

	// 立即模式：先提交批处理队列，保证绘制顺序正确
	FlushBatch();

	m_geomShader.use();

	// 设置投影矩阵、视图矩阵和当前变换矩阵
	glUniformMatrix4fv(m_geomShader.getUniformLocation("proj"), 1, GL_FALSE, glm::value_ptr(m_projection));
	glUniformMatrix4fv(m_geomShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));
	glUniformMatrix4fv(m_geomShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(m_transformStack.back()));

	// 上传顶点数据
	BindVAO(m_geomVAO);
	glBindBuffer(GL_ARRAY_BUFFER, m_geomVBO);

	if (vertices.size() > m_geomBatchCapacity) {
		m_geomBatchCapacity = vertices.size() * 2;
		glBufferData(GL_ARRAY_BUFFER, m_geomBatchCapacity * sizeof(GeomVertex), nullptr, GL_DYNAMIC_DRAW);
	}
	glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(GeomVertex), vertices.data());

	glDrawArrays(mode, 0, (GLsizei)vertices.size());
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