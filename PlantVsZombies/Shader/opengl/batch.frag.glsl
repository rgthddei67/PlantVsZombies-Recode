#version 330 core

in vec2 texCoord;
in vec4 color;
flat in vec4 clipRect;

uniform sampler2D uTexture;
uniform float uFramebufferHeight;

out vec4 outColor;

void main()
{
    vec2 topLeftFragment = vec2(gl_FragCoord.x, uFramebufferHeight - gl_FragCoord.y);
    if (topLeftFragment.x < clipRect.x || topLeftFragment.y < clipRect.y ||
        topLeftFragment.x >= clipRect.z || topLeftFragment.y >= clipRect.w)
    {
        discard;
    }

    vec4 sampled = texture(uTexture, texCoord);
    outColor = vec4(sampled.rgb * color.rgb * color.a, sampled.a * color.a);
}
