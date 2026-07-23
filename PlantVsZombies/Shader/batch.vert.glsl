#version 450

// Phase 3b — bindless 批渲染顶点着色器。
//
// 顶点格式 (stride=52B，与 C++ 端 struct BatchVertex 对齐):
//   loc 0  vec2  pos          @  0
//   loc 1  vec2  uv           @  8
//   loc 2  uvec2 indices      @ 16   (.x=texIndex, .y=matrixIndex)
//   loc 3  vec4  color        @ 24
//   (blendMode @40 仅 CPU 侧分段使用，shader 不读)
//   loc 4  uvec2 packedClip   @44   (.x=left|top<<16, .y=right|bottom<<16)
//
// set=0 binding=0  矩阵 SSBO（逐帧持久映射 host-visible，由 Graphics 写入）
// push_constant    projView mat4（每帧一次）
layout(set = 0, binding = 0) readonly buffer MatrixBlock {
    mat4 m[];
} matrices;

layout(push_constant) uniform PC {
    mat4 projView;
} pc;

layout(location = 0) in vec2  inPos;
layout(location = 1) in vec2  inUV;
layout(location = 2) in uvec2 inIndices;
layout(location = 3) in vec4  inColor;
layout(location = 4) in uvec2 inPackedClip;

layout(location = 0) out vec2      vUV;
layout(location = 1) out vec4      vColor;
layout(location = 2) flat out uint vTex;
layout(location = 3) flat out uvec2 vPackedClip;

void main() {
    mat4 model = matrices.m[inIndices.y];
    vec4 logical = model * vec4(inPos, 0.0, 1.0);
    gl_Position = pc.projView * logical;
    vUV    = inUV;
    vColor = inColor;
    vTex   = inIndices.x;
    vPackedClip = inPackedClip;
}
