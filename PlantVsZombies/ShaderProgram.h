#pragma once
#ifndef __SHADER_PROGRAM_H__
#define __SHADER_PROGRAM_H__

#include <glad/glad.h>
#include <string>
#include <unordered_map>

class ShaderProgram {
public:
    enum ShaderType {
        VERTEX_SHADER,
        FRAGMENT_SHADER
    };

public:
    ShaderProgram();
    ~ShaderProgram();

    bool loadFromFile(const char* vertexPath, const char* fragmentPath);
    bool loadFromFile(const char* vertexPath, const char* fragmentPath,
                      const std::string& defines);
    void use() const;
    GLuint getProgramID() const { return m_programID; }
    GLint getUniformLocation(const char* name) const;

private:
    GLuint compileShader(const char* path, ShaderType type,
                         const std::string& defines = std::string());

private:
    GLuint m_programID;
    mutable std::unordered_map<std::string, GLint> m_uniformCache;
};

#endif