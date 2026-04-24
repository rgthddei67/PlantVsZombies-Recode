#include "ShaderProgram.h"
#include "FileManager.h"
#include <iostream>
#include <fstream>
#include <sstream>

#define GL_CALL(x) do { x; } while(0)

ShaderProgram::ShaderProgram() : m_programID(0) {}

ShaderProgram::~ShaderProgram() {
    if (m_programID) {
        glDeleteProgram(m_programID);
    }
}

bool ShaderProgram::loadFromFile(const char* vertexPath, const char* fragmentPath) {
    return loadFromFile(vertexPath, fragmentPath, std::string());
}

bool ShaderProgram::loadFromFile(const char* vertexPath, const char* fragmentPath,
                                 const std::string& defines) {
    GLuint vertexShader = compileShader(vertexPath, VERTEX_SHADER, defines);
    GLuint fragmentShader = compileShader(fragmentPath, FRAGMENT_SHADER, defines);

    if (vertexShader == 0 || fragmentShader == 0) {
        return false;
    }

    m_programID = glCreateProgram();
    GL_CALL(glAttachShader(m_programID, vertexShader));
    GL_CALL(glAttachShader(m_programID, fragmentShader));

    GL_CALL(glLinkProgram(m_programID));

    GLint success = 0;
    GL_CALL(glGetProgramiv(m_programID, GL_LINK_STATUS, &success));
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(m_programID, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Error linking shader program: " << infoLog << std::endl;
        glDeleteProgram(m_programID);
        m_programID = 0;
        return false;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return true;
}

void ShaderProgram::use() const {
    if (m_programID) {
        glUseProgram(m_programID);
    }
}

GLint ShaderProgram::getUniformLocation(const char* name) const {
    auto it = m_uniformCache.find(name);
    if (it != m_uniformCache.end()) return it->second;
    GLint loc = glGetUniformLocation(m_programID, name);
    m_uniformCache[name] = loc;
    return loc;
}

GLuint ShaderProgram::compileShader(const char* path, ShaderType type,
                                    const std::string& defines) {
    std::string file = FileManager::LoadFileAsString(path);
    if (file.empty()) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return 0;
    }

    std::string source = file;
    if (!defines.empty()) {
        // 在 #version ... 行之后插入 defines；若找不到 #version 则前置
        size_t versionPos = source.find("#version");
        if (versionPos != std::string::npos) {
            size_t lineEnd = source.find('\n', versionPos);
            if (lineEnd != std::string::npos) {
                source.insert(lineEnd + 1, defines);
            }
            else {
                source += "\n" + defines;
            }
        }
        else {
            source = defines + source;
        }
    }
    const char* sourceCStr = source.c_str();

    GLenum glShaderType = (type == VERTEX_SHADER) ? GL_VERTEX_SHADER : GL_FRAGMENT_SHADER;
    GLuint shader = glCreateShader(glShaderType);
    glShaderSource(shader, 1, &sourceCStr, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
        std::cerr << "Error compiling shader (" << path << "): " << infoLog << std::endl;
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}