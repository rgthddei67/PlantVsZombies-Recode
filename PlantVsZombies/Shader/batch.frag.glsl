#version 450
#extension GL_EXT_nonuniform_qualifier : require

// set=1 binding=1  bindless 纹理数组（8192 槽位，由 VulkanTexturePool 拥有）
layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2      vUV;
layout(location = 1) in vec4      vColor;
layout(location = 2) flat in uint vTex;
layout(location = 3) in float      vLogicalY;
layout(location = 4) flat in float vClipBottomY;

layout(location = 0) out vec4 outColor;

void main() {
    if (vLogicalY > vClipBottomY) {
        discard;
    }
    // 纹理为预乘 alpha；vColor 仍是直通语义（tint.rgb + 淡入淡出 alpha）。
    // 把 vColor.a 也预乘进 rgb，输出即为预乘色，配合 srcColorBlendFactor=ONE 的混合。
    vec4 t = texture(textures[nonuniformEXT(vTex)], vUV);
    outColor = vec4(t.rgb * vColor.rgb * vColor.a, t.a * vColor.a);
}
