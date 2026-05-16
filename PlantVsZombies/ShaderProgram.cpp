#include "ShaderProgram.h"

// Phase 3a stub：所有方法 no-op，避免在没有 GL 上下文的情况下调用 gl* 段错误。
// Phase 6 删除整个 ShaderProgram.{h,cpp}（被 VulkanPipeline 替代）。

ShaderProgram::ShaderProgram() : m_programID(0) {}

ShaderProgram::~ShaderProgram() {}

bool ShaderProgram::loadFromFile(const char* /*vertexPath*/, const char* /*fragmentPath*/) {
    return true;
}

bool ShaderProgram::loadFromFile(const char* /*vertexPath*/, const char* /*fragmentPath*/,
                                 const std::string& /*defines*/) {
    return true;
}

void ShaderProgram::use() const {}

GLint ShaderProgram::getUniformLocation(const char* /*name*/) const {
    return -1;
}

GLuint ShaderProgram::compileShader(const char* /*path*/, ShaderType /*type*/,
                                    const std::string& /*defines*/) {
    return 0;
}
