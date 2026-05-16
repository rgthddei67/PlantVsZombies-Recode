#version 450
#extension GL_EXT_nonuniform_qualifier : require

// Bindless 纹理数组。VARIABLE_DESCRIPTOR_COUNT 在 C++ 侧分配为 4096。
layout(set = 0, binding = 1) uniform sampler2D textures[];

layout(location = 0) in vec2 vUV;
layout(location = 1) flat in uint vTex;
layout(location = 0) out vec4 outColor;

void main() {
    outColor = texture(textures[nonuniformEXT(vTex)], vUV);
}
