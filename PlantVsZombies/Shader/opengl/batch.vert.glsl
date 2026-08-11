#version 330 core

layout(location = 0) in vec2 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inColor;
layout(location = 3) in vec4 inClipRect;

uniform mat4 uProjectionView;

out vec2 texCoord;
out vec4 color;
flat out vec4 clipRect;

void main()
{
    gl_Position = uProjectionView * vec4(inPosition, 0.0, 1.0);
    texCoord = inTexCoord;
    color = inColor;
    clipRect = inClipRect;
}
