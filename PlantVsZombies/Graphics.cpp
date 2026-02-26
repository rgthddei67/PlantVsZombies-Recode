#include "Graphics.h"

Graphics::Graphics() {
    m_transformStack.push_back(glm::mat4(1.0f));
    this->SetBlendMode(BlendMode::Alpha);
}

Graphics::~Graphics() {
    FlushBatch();                       // 提交剩余顶点
    ClearTextCache();                    // 释放文字纹理

    if (m_spriteVAO) glDeleteVertexArrays(1, &m_spriteVAO);
    if (m_spriteVBO) glDeleteBuffers(1, &m_spriteVBO);
    if (m_spriteEBO) glDeleteBuffers(1, &m_spriteEBO);

    if (m_batchVAO) glDeleteVertexArrays(1, &m_batchVAO);
    if (m_batchVBO) glDeleteBuffers(1, &m_batchVBO);
    if (m_matrixUBO) glDeleteBuffers(1, &m_matrixUBO);

    // 释放几何图形渲染资源
    if (m_geomVAO) glDeleteVertexArrays(1, &m_geomVAO);
    if (m_geomVBO) glDeleteBuffers(1, &m_geomVBO);
}

bool Graphics::Initialize(int windowWidth, int windowHeight) {
    // 查询最大纹理单元数，并限制不超过 32（着色器数组大小）
    glGetIntegerv(GL_MAX_TEXTURE_IMAGE_UNITS, &m_maxTextureUnits);
    if (m_maxTextureUnits > 32) m_maxTextureUnits = 32;

    if (!InitShaders()) return false;
    if (!InitSpriteRenderer()) return false;
    if (!InitGeomRenderer()) return false;

    SetWindowSize(windowWidth, windowHeight);

    // 初始化批处理 VAO 和 VBO
    glGenVertexArrays(1, &m_batchVAO);
    glGenBuffers(1, &m_batchVBO);

    // 初始容量设为 VERTEX_BATCH_LIMIT
    m_batchBufferCapacity = VERTEX_BATCH_LIMIT;
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
    // 纹理索引
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)(4 * sizeof(float)));
    glEnableVertexAttribArray(2);
    // 矩阵索引
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(3);
    // 颜色
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(BatchVertex), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(4);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    // 创建矩阵 UBO，绑定到 binding point 0
    glGenBuffers(1, &m_matrixUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_matrixUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::mat4) * MATRIX_BATCH_LIMIT, nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_matrixUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // 将 batch shader 的 MatrixBlock 绑定到 binding point 0
    GLuint blockIndex = glGetUniformBlockIndex(m_batchShader.getProgramID(), "MatrixBlock");
    if (blockIndex == GL_INVALID_INDEX) {
        std::cerr << "[Graphics] MatrixBlock not found in batch shader (UBO binding failed)" << std::endl;
        return false;
    }
    glUniformBlockBinding(m_batchShader.getProgramID(), blockIndex, 0);

    return true;
}

bool Graphics::InitShaders() {
    // 加载精灵着色器（立即模式）
    if (!m_spriteShader.loadFromFile("./Shader/sprite_vertex.glsl", "./Shader/sprite_fragment.glsl")) {
        std::cerr << "[Graphics] Failed to load sprite shader." << std::endl;
        return false;
    }

    // 加载批处理着色器
    if (!m_batchShader.loadFromFile("./Shader/batch_vertex.glsl", "./Shader/batch_fragment.glsl")) {
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
    return true;
}

void Graphics::SetWindowSize(int width, int height) {
    m_windowWidth  = width;
    m_windowHeight = height;
    m_projection = glm::ortho(0.0f, (float)width, (float)height, 0.0f, -1.0f, 1.0f);
    glViewport(0, 0, width, height);
}

void Graphics::PushTransform(const glm::mat4& transform) {
    glm::mat4 newTop = m_transformStack.back() * transform;
    m_transformStack.push_back(newTop);
}

void Graphics::PopTransform() {
    if (m_transformStack.size() > 1) {
        m_transformStack.pop_back();
    }
    else {
        std::cerr << "[Graphics] PopTransform failed: stack underflow." << std::endl;
    }
}

void Graphics::SetIdentity() {
    if (!m_transformStack.empty())
        m_transformStack.back() = glm::mat4(1.0f);
}

void Graphics::Translate(float x, float y, float z) {
    if (m_transformStack.empty()) return;
    m_transformStack.back() = glm::translate(m_transformStack.back(), glm::vec3(x, y, z));
}

void Graphics::Rotate(float angleDegrees, float x, float y, float z) {
    if (m_transformStack.empty()) return;
    m_transformStack.back() = glm::rotate(m_transformStack.back(), glm::radians(angleDegrees), glm::vec3(x, y, z));
}

void Graphics::Scale(float sx, float sy, float sz) {
    if (m_transformStack.empty()) return;
    m_transformStack.back() = glm::scale(m_transformStack.back(), glm::vec3(sx, sy, sz));
}

void Graphics::BeginBatch() {
    // 自动累积，无需操作
}

void Graphics::EndBatch() {
    FlushBatch();
}

void Graphics::FlushBatch() {
    if (!m_batchVertices.empty()) {
        // 确保 VBO 容量足够
        if (m_batchVertices.size() > m_batchBufferCapacity) {
            size_t newCapacity = m_batchVertices.size() * 2;
            ResizeBatchBuffer(newCapacity);
        }

        // 使用批处理着色器
        m_batchShader.use();

        // 上传投影矩阵
        glUniformMatrix4fv(m_batchShader.getUniformLocation("proj"), 1, GL_FALSE, glm::value_ptr(m_projection));

        // 上传所有变换矩阵到 UBO
        if (!m_batchMatrices.empty()) {
            glBindBuffer(GL_UNIFORM_BUFFER, m_matrixUBO);
            glBufferSubData(GL_UNIFORM_BUFFER, 0,
                m_batchMatrices.size() * sizeof(glm::mat4),
                glm::value_ptr(m_batchMatrices[0]));
            glBindBuffer(GL_UNIFORM_BUFFER, 0);
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

        // 绘制
        glBindVertexArray(m_batchVAO);
        glDrawArrays(GL_TRIANGLES, 0, (GLsizei)m_batchVertices.size());
        glBindVertexArray(0);

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
    if ((int)m_batchMatrices.size() >= MATRIX_BATCH_LIMIT) {
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
    if ((int)m_batchVertices.size() >= VERTEX_BATCH_LIMIT ||
        (int)m_batchMatrices.size() >= MATRIX_BATCH_LIMIT ||
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

        // 构建两个三角形（6个顶点）的批处理顶点数据
        BatchVertex vertices[6] = {
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 1.0f, 1.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 0.0f, 0.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a}
        };
        AddVertices(vertices, 6);
        CheckBatch();
    }
    else {
        // 立即模式绘制
        m_spriteShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
        glUniform4f(m_spriteShader.getUniformLocation("color"), tint.r, tint.g, tint.b, tint.a);

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
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"),      1, GL_FALSE, glm::value_ptr(finalModel));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"),       1, GL_FALSE, glm::value_ptr(m_viewMatrix));

        glBindVertexArray(m_spriteVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Graphics::DrawTextureMatrix(const GLTexture* tex, const glm::mat4& transform,
    float pivotX, float pivotY, const glm::vec4& tint) {

    if (!tex) return;

    if (m_batchMode) {
        int texIndex = BindTexture(tex->id);

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
        glm::mat4 finalMatrix = m_transformStack.back() * pivotTransform;
        int matrixIndex = AddMatrix(finalMatrix);

        BatchVertex vertices[6] = {
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 1.0f, 1.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 0.0f, 0.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a}
        };
        AddVertices(vertices, 6);
        CheckBatch();
    }
    else {
        // 立即模式
        m_spriteShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
        glUniform4f(m_spriteShader.getUniformLocation("color"), tint.r, tint.g, tint.b, tint.a);

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
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"),      1, GL_FALSE, glm::value_ptr(finalModel));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"),       1, GL_FALSE, glm::value_ptr(m_viewMatrix));

        glBindVertexArray(m_spriteVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Graphics::DrawTextureRegion(const GLTexture* tex,
    float srcX, float srcY, float srcW, float srcH,
    float dstX, float dstY, float dstW, float dstH,
    float rotation, const glm::vec4& tint)
{
    if (!tex || tex->id == 0) return;

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

        BatchVertex vertices[6] = {
            {0.0f, 1.0f, u0, v1, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 1.0f, u1, v1, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, u1, v0, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 1.0f, u0, v1, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {0.0f, 0.0f, u0, v0, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a},
            {1.0f, 0.0f, u1, v0, (float)texIndex, (float)matrixIndex, tint.r, tint.g, tint.b, tint.a}
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

        m_spriteShader.use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex->id);
        glUniform1i(m_spriteShader.getUniformLocation("texture1"), 0);
        glUniform4f(m_spriteShader.getUniformLocation("color"), tint.r, tint.g, tint.b, tint.a);

        glm::mat4 finalModel = m_transformStack.back() * local;
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(finalModel));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"), 1, GL_FALSE, glm::value_ptr(m_viewMatrix));

        glBindVertexArray(m_spriteVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);

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
        outWidth  = it->second.first.width;
        outHeight = it->second.first.height;
        return it->second.first.textureID;
    }

    TTF_Font* font = ResourceManager::GetInstance().GetFont(fontKey, fontSize);
    if (!font) {
        std::cerr << "[GetOrCreateTextTexture] Failed to get font: " << fontKey << " size " << fontSize << std::endl;
        return 0;
    }

    SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text.c_str(), ToSDLColor(color));
    if (!surface) {
        std::cerr << "[GetOrCreateTextTexture] TTF_RenderText_Blended error: " << TTF_GetError() << std::endl;
        return 0;
    }

    // 转换为 ABGR8888 格式以便 OpenGL 使用
    SDL_Surface* rgbaSurface = SDL_ConvertSurfaceFormat(surface, SDL_PIXELFORMAT_ABGR8888, 0);
    SDL_FreeSurface(surface);
    if (!rgbaSurface) {
        std::cerr << "Convert surface error: " << SDL_GetError() << std::endl;
        return 0;
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

    outWidth = rgbaSurface->w;
    outHeight = rgbaSurface->h;
    SDL_FreeSurface(rgbaSurface);

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
    m_textCache[key] = { {textureID, outWidth, outHeight}, m_textCacheOrder.begin() };
    return textureID;
}

void Graphics::DrawText(const std::string& text, const std::string& fontKey, int fontSize,
    const glm::vec4& color, float x, float y, float scale) {
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
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1},
            {1.0f, 1.0f, 1.0f, 1.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1},
            {0.0f, 1.0f, 0.0f, 1.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1},
            {0.0f, 0.0f, 0.0f, 0.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1},
            {1.0f, 0.0f, 1.0f, 0.0f, (float)texIndex, (float)matrixIndex, 1,1,1,1}
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
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("model"),      1, GL_FALSE, glm::value_ptr(finalModel));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("projection"), 1, GL_FALSE, glm::value_ptr(m_projection));
        glUniformMatrix4fv(m_spriteShader.getUniformLocation("view"),       1, GL_FALSE, glm::value_ptr(m_viewMatrix));

        glBindVertexArray(m_spriteVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
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
    glClearColor(r, g, b, a);
}

void Graphics::SetBlendMode(BlendMode mode) {
    // 提交当前批次，避免混合状态混乱
    if (!m_batchVertices.empty() || !m_geomBatchVertices.empty()) {
        FlushBatch();
    }

    m_currentBlendMode = mode;

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
    m_cameraPos      = glm::vec2(0.0f);
    m_cameraZoom     = 1.0f;
    m_cameraRotation = 0.0f;
    m_viewMatrix     = glm::mat4(1.0f);
}

glm::vec2 Graphics::ScreenToWorldPosition(float screenX, float screenY) const {
    // 屏幕坐标 → NDC（注意 Y 轴翻转，因为正交投影 top=0）
    float ndcX = (screenX / m_windowWidth)  *  2.0f - 1.0f;
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
    glUniformMatrix4fv(m_geomShader.getUniformLocation("proj"),  1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(m_geomShader.getUniformLocation("view"),  1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(m_geomShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(identity));

    glBindVertexArray(m_geomVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_geomVBO);

    // 按需扩容
    if (m_geomBatchVertices.size() > m_geomBatchCapacity) {
        m_geomBatchCapacity = m_geomBatchVertices.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, m_geomBatchCapacity * sizeof(GeomVertex), nullptr, GL_DYNAMIC_DRAW);
    }

    glBufferSubData(GL_ARRAY_BUFFER, 0, m_geomBatchVertices.size() * sizeof(GeomVertex), m_geomBatchVertices.data());
    glDrawArrays(m_geomBatchMode, 0, (GLsizei)m_geomBatchVertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    m_geomBatchVertices.clear();
}

void Graphics::ResizeBatchBuffer(size_t newCapacity) {
    // 暂存当前数据
    std::vector<BatchVertex> oldData;
    if (!m_batchVertices.empty()) {
        oldData = m_batchVertices;
    }

    // 重新分配 VBO
    glBindBuffer(GL_ARRAY_BUFFER, m_batchVBO);
    glBufferData(GL_ARRAY_BUFFER, newCapacity * sizeof(BatchVertex), nullptr, GL_DYNAMIC_DRAW);

    // 恢复数据（如果有）
    if (!oldData.empty()) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, oldData.size() * sizeof(BatchVertex), oldData.data());
    }

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
                const auto& v0  = vertices[0];
                const auto& vi  = vertices[i];
                const auto& vi1 = vertices[i + 1];
                glm::vec4 p0  = transform * glm::vec4(v0.x,  v0.y,  0.0f, 1.0f);
                glm::vec4 pi  = transform * glm::vec4(vi.x,  vi.y,  0.0f, 1.0f);
                glm::vec4 pi1 = transform * glm::vec4(vi1.x, vi1.y, 0.0f, 1.0f);
                m_geomBatchVertices.push_back({ p0.x,  p0.y,  v0.r,  v0.g,  v0.b,  v0.a  });
                m_geomBatchVertices.push_back({ pi.x,  pi.y,  vi.r,  vi.g,  vi.b,  vi.a  });
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
    glUniformMatrix4fv(m_geomShader.getUniformLocation("proj"),  1, GL_FALSE, glm::value_ptr(m_projection));
    glUniformMatrix4fv(m_geomShader.getUniformLocation("view"),  1, GL_FALSE, glm::value_ptr(m_viewMatrix));
    glUniformMatrix4fv(m_geomShader.getUniformLocation("model"), 1, GL_FALSE, glm::value_ptr(m_transformStack.back()));

    // 上传顶点数据
    glBindVertexArray(m_geomVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_geomVBO);

    if (vertices.size() > m_geomBatchCapacity) {
        m_geomBatchCapacity = vertices.size() * 2;
        glBufferData(GL_ARRAY_BUFFER, m_geomBatchCapacity * sizeof(GeomVertex), nullptr, GL_DYNAMIC_DRAW);
    }
    glBufferSubData(GL_ARRAY_BUFFER, 0, vertices.size() * sizeof(GeomVertex), vertices.data());

    glDrawArrays(mode, 0, (GLsizei)vertices.size());

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Graphics::DrawLine(float x1, float y1, float x2, float y2, const glm::vec4& color) {
    std::vector<GeomVertex> verts = {
        {x1, y1, color.r, color.g, color.b, color.a},
        {x2, y2, color.r, color.g, color.b, color.a}
    };
    DrawGeomImmediate(verts, GL_LINES);
}

void Graphics::DrawRect(float x, float y, float width, float height, const glm::vec4& color) {
    float r = color.r, g = color.g, b = color.b, a = color.a;
    std::vector<GeomVertex> verts = {
        {x,         y,          r, g, b, a},  // 左上
        {x + width, y,          r, g, b, a},  // 右上
        {x + width, y + height, r, g, b, a},  // 右下
        {x,         y + height, r, g, b, a}   // 左下
    };
    DrawGeomImmediate(verts, GL_LINE_LOOP);
}

void Graphics::FillRect(float x, float y, float width, float height, const glm::vec4& color) {
    float r = color.r, g = color.g, b = color.b, a = color.a;
    std::vector<GeomVertex> verts = {
        {x,         y,          r, g, b, a},  // 左上
        {x + width, y,          r, g, b, a},  // 右上
        {x + width, y + height, r, g, b, a},  // 右下
        {x,         y,          r, g, b, a},  // 左上（第二个三角形）
        {x + width, y + height, r, g, b, a},  // 右下
        {x,         y + height, r, g, b, a}   // 左下
    };
    DrawGeomImmediate(verts, GL_TRIANGLES);
}

void Graphics::DrawCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
    std::vector<GeomVertex> verts;
    verts.reserve(segments);
    float r = color.r, g = color.g, b = color.b, a = color.a;
    for (int i = 0; i < segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        verts.push_back({ cx + radius * cosf(angle), cy + radius * sinf(angle), r, g, b, a });
    }
    DrawGeomImmediate(verts, GL_LINE_LOOP);
}

void Graphics::FillCircle(float cx, float cy, float radius, const glm::vec4& color, int segments) {
    std::vector<GeomVertex> verts;
    verts.reserve(segments + 2);
    float r = color.r, g = color.g, b = color.b, a = color.a;
    // 圆心
    verts.push_back({ cx, cy, r, g, b, a });
    // 圆周上的点
    for (int i = 0; i <= segments; ++i) {
        float angle = 2.0f * glm::pi<float>() * i / segments;
        verts.push_back({ cx + radius * cosf(angle), cy + radius * sinf(angle), r, g, b, a });
    }
    DrawGeomImmediate(verts, GL_TRIANGLE_FAN);
}

void Graphics::Submit(std::function<void()> cmd) {
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
    float pivotX, float pivotY, const glm::vec4& tint) {
    Submit([=]() { DrawTextureMatrix(texture, transform, pivotX, pivotY, tint); });
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