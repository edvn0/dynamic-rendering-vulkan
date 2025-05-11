#version 460

#include "set0.glsl"

layout(location = 0) in vec3 inPos;

layout(location = 0) out vec3 outUVW;

void main()
{
    outUVW = inPos;
    outUVW.xy *= -1.0;
    mat4 viewMat = mat4(mat3(camera_ubo.view));
    gl_Position = camera_ubo.projection * viewMat * vec4(inPos.xyz, 1.0);
}
