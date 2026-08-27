#version 450

// GPU instancing vertex shader for reanim sprites.
//
// Inputs:
//   locations 0..4 → one InstanceRecord through a per-instance vertex binding
//   gl_VertexIndex 0..3 → quad corner + UV (triangle strip, Y-down)
//
// push_constant     projView mat4
//
// Vertex index → corner mapping:
//   idx 0 → (0,0)  TL    UV (u0,v0)
//   idx 1 → (0,1)  BL    UV (u0,v1)
//   idx 2 → (1,0)  TR    UV (u1,v0)
//   idx 3 → (1,1)  BR    UV (u1,v1)

layout(location = 0) in vec4  iTransform;
layout(location = 1) in vec4  iTranslateUv0;
layout(location = 2) in vec2  iUv1;
layout(location = 3) in uint  iTexSlot;
layout(location = 4) in vec4  iColor;

layout(push_constant) uniform PC {
    mat4 projView;
} pc;

layout(location = 0) out vec2      vUV;
layout(location = 1) flat out vec4 vColor;
layout(location = 2) flat out uint vTex;

void main() {
    vec2 corner;
    vec2 uv;
    switch (gl_VertexIndex) {
        case 0: corner = vec2(0.0, 0.0); uv = iTranslateUv0.zw; break;
        case 1: corner = vec2(0.0, 1.0); uv = vec2(iTranslateUv0.z, iUv1.y); break;
        case 2: corner = vec2(1.0, 0.0); uv = vec2(iUv1.x, iTranslateUv0.w); break;
        default: corner = vec2(1.0, 1.0); uv = iUv1; break;
    }

    // Apply 2x3 affine (columns: (tA,tB) and (tC,tD); translation (tx,ty)).
    // tA..tD are pre-multiplied on the CPU side by (sprite_width × Scale) and
    // (sprite_height × Scale), so `corner` here is unit-quad.
    vec2 local = vec2(
        iTransform.x * corner.x + iTransform.z * corner.y + iTranslateUv0.x,
        iTransform.y * corner.x + iTransform.w * corner.y + iTranslateUv0.y
    );

    gl_Position = pc.projView * vec4(local, 0.0, 1.0);

    vUV  = uv;
    vTex = iTexSlot;
    // CPU 以小端 RGBA8 存放；顶点格式直接归一化为 0..1。
    vColor = iColor;
}
