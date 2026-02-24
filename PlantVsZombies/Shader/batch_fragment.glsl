#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
flat in int TexIndex;
in vec4 Color;

uniform sampler2D textures[32];  // 最大支持32个纹理单元

void main() {
    // 根据纹理索引从纹理数组中采样
    vec4 texColor = texture(textures[TexIndex], TexCoord);
    FragColor = texColor * Color;   // 应用顶点颜色调制
}