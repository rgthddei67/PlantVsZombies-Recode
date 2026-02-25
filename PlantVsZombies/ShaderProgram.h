#pragma once
#ifndef __SHADER_PROGRAM_H__
#define __SHADER_PROGRAM_H__

#include <glad/glad.h>
#include <string>

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
    void use() const;
    GLuint getProgramID() const { return m_programID; }
    GLint getUniformLocation(const char* name) const;

private:
    GLuint compileShader(const char* path, ShaderType type);

private:
    GLuint m_programID;
};

#endif