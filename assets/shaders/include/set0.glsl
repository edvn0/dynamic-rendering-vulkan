#ifndef _SET0_GLSL_
#define _SET0_GLSL_

layout(set = 0, binding = 0) uniform CameraUBO
{
    mat4 vp;
    mat4 inverse_vp;
    vec3 camera_position;
    float _padding_;
    vec4 _padding_2[3];
}
camera_ubo;

layout(set = 0, binding = 1, std140) uniform ShadowUBO
{
    mat4 light_vp;
    vec4 light_position;
    vec4 light_color;
    vec4 ambient_color;
    vec4 _padding_[1];
}
shadow_ubo;

layout(set = 0, binding = 2, std140) uniform CameraFrustumPlanes
{
    vec4 planes[6];
    vec4 _padding_[2];
}
frustum_ubo;

layout(set = 0, binding = 3) uniform sampler2DShadow shadow_image;

#endif