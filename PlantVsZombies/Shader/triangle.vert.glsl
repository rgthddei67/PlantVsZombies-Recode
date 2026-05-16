#version 450

// Phase 2a 测试三角形：3 个顶点的 NDC 位置和颜色硬编码，
// 通过 gl_VertexIndex 索引——不需要绑定 vertex buffer。
const vec2 positions[3] = vec2[3](
    vec2( 0.0, -0.6),
    vec2( 0.6,  0.6),
    vec2(-0.6,  0.6)
);
const vec3 colors[3] = vec3[3](
    vec3(1.0, 0.0, 0.0),
    vec3(0.0, 1.0, 0.0),
    vec3(0.0, 0.0, 1.0)
);

layout(location = 0) out vec3 vColor;

void main() {
    gl_Position = vec4(positions[gl_VertexIndex], 0.0, 1.0);
    vColor = colors[gl_VertexIndex];
}
