#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in uint aTexIndex;
layout (location = 3) in uint aMatrixIndex;
layout (location = 4) in vec4 aColor;   // tint

out vec2 TexCoord;
flat out int TexIndex;
out vec4 Color;

uniform mat4 proj;
uniform mat4 view;   // 摄像机视图矩阵

#ifdef USE_SSBO
layout(std430, binding = 0) buffer MatrixBlock {
    mat4 posMatrix[];
};
#else
#ifndef MATRIX_BATCH_LIMIT
#define MATRIX_BATCH_LIMIT 256
#endif

layout(std140) uniform MatrixBlock {
    mat4 posMatrix[MATRIX_BATCH_LIMIT];
};
#endif

void main() {
    mat4 model = posMatrix[aMatrixIndex];
    gl_Position = proj * view * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
    TexIndex = int(aTexIndex);
    Color = aColor;
}
