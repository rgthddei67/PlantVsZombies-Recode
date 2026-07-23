#version 450

// GPU instancing vertex shader for reanim sprites.
//
// Inputs:
//   gl_InstanceIndex → InstanceRecord in SSBO at set=0 binding=0
//   gl_VertexIndex 0..5 → quad corner + UV (two triangles, CCW with Y-down)
//
// set=0 binding=0   readonly buffer of InstanceRecord (host-mapped)
// push_constant     projView mat4
//
// Vertex index → corner mapping:
//   idx 0 → (0,0)  TL    UV (u0,v0)
//   idx 1 → (0,1)  BL    UV (u0,v1)
//   idx 2 → (1,1)  BR    UV (u1,v1)
//   idx 3 → (0,0)  TL    UV (u0,v0)
//   idx 4 → (1,1)  BR    UV (u1,v1)
//   idx 5 → (1,0)  TR    UV (u1,v0)

struct InstanceRecord {
    float tA, tB, tC, tD;
    float tx, ty;
    float u0, v0;
    float u1, v1;
    uint  texSlot;
    uint  colorRGBA8;
    float clipBottomY;
};

layout(set = 0, binding = 0) readonly buffer InstanceBlock {
    InstanceRecord inst[];
} instances;

layout(push_constant) uniform PC {
    mat4 projView;
} pc;

layout(location = 0) out vec2      vUV;
layout(location = 1) out vec4      vColor;
layout(location = 2) flat out uint vTex;
layout(location = 3) out float      vLogicalY;
layout(location = 4) flat out float vClipBottomY;

void main() {
    InstanceRecord r = instances.inst[gl_InstanceIndex];

    vec2 corner;
    vec2 uv;
    switch (gl_VertexIndex) {
        case 0: corner = vec2(0.0, 0.0); uv = vec2(r.u0, r.v0); break;
        case 1: corner = vec2(0.0, 1.0); uv = vec2(r.u0, r.v1); break;
        case 2: corner = vec2(1.0, 1.0); uv = vec2(r.u1, r.v1); break;
        case 3: corner = vec2(0.0, 0.0); uv = vec2(r.u0, r.v0); break;
        case 4: corner = vec2(1.0, 1.0); uv = vec2(r.u1, r.v1); break;
        case 5: corner = vec2(1.0, 0.0); uv = vec2(r.u1, r.v0); break;
    }

    // Apply 2x3 affine (columns: (tA,tB) and (tC,tD); translation (tx,ty)).
    // tA..tD are pre-multiplied on the CPU side by (sprite_width × Scale) and
    // (sprite_height × Scale), so `corner` here is unit-quad.
    vec2 local = vec2(
        r.tA * corner.x + r.tC * corner.y + r.tx,
        r.tB * corner.x + r.tD * corner.y + r.ty
    );

    gl_Position = pc.projView * vec4(local, 0.0, 1.0);

    vUV  = uv;
    vTex = r.texSlot;
    vLogicalY = local.y;
    vClipBottomY = r.clipBottomY;
    // Unpack RGBA8 (r=lsb, a=msb) — matches CPU pack convention to be added in Task 5.
    vColor = vec4(
        float((r.colorRGBA8      ) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >>  8) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >> 16) & 0xFFu) / 255.0,
        float((r.colorRGBA8 >> 24) & 0xFFu) / 255.0
    );
}
