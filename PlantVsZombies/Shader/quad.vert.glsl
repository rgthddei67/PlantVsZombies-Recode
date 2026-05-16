#version 450

// 一个不需要 vertex buffer 的 quad：6 个三角形顶点用 gl_VertexIndex 索引下面的常量表。
layout(push_constant) uniform PC {
    vec4 rect;       // x, y, w, h in NDC（左下原点）
    uint texIndex;
} pc;

const vec2 verts[6] = vec2[6](
    vec2(0.0, 0.0), vec2(1.0, 0.0), vec2(1.0, 1.0),
    vec2(0.0, 0.0), vec2(1.0, 1.0), vec2(0.0, 1.0)
);

layout(location = 0) out vec2 vUV;
layout(location = 1) flat out uint vTex;

void main() {
    vec2 uv = verts[gl_VertexIndex];
    vec2 pos = pc.rect.xy + uv * pc.rect.zw;
    gl_Position = vec4(pos, 0.0, 1.0);
    vUV = uv;
    vTex = pc.texIndex;
}
