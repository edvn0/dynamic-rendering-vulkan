#ifndef _SET0_GLSL_
#define _SET0_GLSL_

layout(std140, set = 0, binding = 0) uniform CameraUBO
{
    mat4 vp;
    mat4 inverse_vp;
    mat4 projection;
    mat4 view;
    mat4 inverse_projection;
    vec4 camera_position;
    vec4 screen_size_near_far;
    float _padding[2];
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

struct PointLight {
    vec3 position;
    float radius;      // occupies the 4th float in the vec4
    vec3 color;
    float intensity;   // also occupies the 4th float in next vec4
};

layout(std140, set = 0, binding = 4) restrict readonly buffer PointLightBuffer {
    uint light_count;
    PointLight lights[];
} point_light_buffer;

#endif