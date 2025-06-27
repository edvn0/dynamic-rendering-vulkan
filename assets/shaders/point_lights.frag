#version 460
#include "set0.glsl"

layout(location = 0) out vec4 fragColor;

layout(set = 1, binding = 0) uniform sampler2D scene_depth;

layout(push_constant) uniform PushConstants {
    vec2 screenSize;
} pc;

// Reconstruct world position from depth
vec3 reconstruct_world_position(vec2 frag_coord, float depth, mat4 inv_vp) {
    vec2 ndc = (frag_coord / pc.screenSize) * 2.0 - 1.0;
    vec4 clip = vec4(ndc, depth, 1.0);
    vec4 world = inv_vp * clip;
    return world.xyz / world.w;
}

void main() {
    vec2 uv = gl_FragCoord.xy / pc.screenSize;
    float depth = 1.0F- texture(scene_depth, uv).r;

    // Inverse of projection * view
    mat4 inv_view_proj = inverse(camera_ubo.projection * camera_ubo.view);
    vec3 world_pos =  reconstruct_world_position(gl_FragCoord.xy, depth, inv_view_proj);

    vec3 color = vec3(0.0);

    for (uint i = 0; i < point_light_buffer.light_count; ++i) {
        PointLight light = point_light_buffer.lights[i];
        float dist = distance(world_pos, light.position);
        if (dist < light.radius) {
            color += light.color * 0.01*light.intensity;
        }
    }

    fragColor = vec4(color, 1.0);
}