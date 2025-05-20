#version 460

#include "set0.glsl"

layout(location = 0) in vec3 in_pos;
layout(location = 0) out vec3 out_uvw;

void main()
{
    vec3 pos = in_pos;
    out_uvw = pos;

    mat4 view = mat4(mat3(camera_ubo.view));
    mat4 proj = camera_ubo.projection;

    gl_Position = proj * view * vec4(pos, 1.0);
    out_uvw.y = -pos.y;
}