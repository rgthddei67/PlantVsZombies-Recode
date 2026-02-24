#version 330 core

in vec4 Color;
out vec4 FragColor;

void main() {
    // 直接输出顶点颜色，不采样纹理
    FragColor = Color;
}
