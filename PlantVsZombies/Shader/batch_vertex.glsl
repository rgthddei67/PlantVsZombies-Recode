#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexIndex;
layout (location = 3) in float aMatrixIndex;
layout (location = 4) in vec4 aColor;   // ������ɫ��tint��

out vec2 TexCoord;
flat out int TexIndex;
out vec4 Color;

uniform mat4 proj;
uniform mat4 view;   // 摄像机视图矩阵
uniform mat4 posMatrix[64];  // ���֧��64������

void main() {
    // ���ݾ�������ѡȡģ�;���
    mat4 model = posMatrix[int(aMatrixIndex)];
    gl_Position = proj * view * model * vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
    TexIndex = int(aTexIndex);
    Color = aColor;
}