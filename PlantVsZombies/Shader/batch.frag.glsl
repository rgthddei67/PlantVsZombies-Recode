#version 450
#extension GL_EXT_nonuniform_qualifier : require

// set=1 binding=1  bindless 纹理数组（4096 槽位，由 VulkanTexturePool 拥有）
layout(set = 1, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2      vUV;
layout(location = 1) in vec4      vColor;
layout(location = 2) flat in uint vTex;

layout(location = 0) out vec4 outColor;

void main() {
    vec4 t = texture(textures[nonuniformEXT(vTex)], vUV);
    outColor = t * vColor;
}
