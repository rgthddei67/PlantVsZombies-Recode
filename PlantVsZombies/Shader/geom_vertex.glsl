#version 330 core

// 顶点位置（屏幕坐标）
layout(location = 0) in vec2 aPos;
// 顶点颜色（RGBA）
layout(location = 1) in vec4 aColor;

// 正交投影矩阵
uniform mat4 proj;
// 摄像机视图矩阵
uniform mat4 view;
// 当前变换矩阵（来自变换栈）
uniform mat4 model;

out vec4 Color;

void main() {
    gl_Position = proj * view * model * vec4(aPos, 0.0, 1.0);
    Color = aColor;
}
