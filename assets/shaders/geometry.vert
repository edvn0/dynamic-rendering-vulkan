#version 460

#include "matrix_math.glsl"
#include "set0.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_tangent;
layout(location = 4) in vec4 a_model_matrix_row0;
layout(location = 5) in vec4 a_model_matrix_row1;
layout(location = 6) in vec4 a_model_matrix_row2;
layout(location = 7) in vec4 a_model_matrix_row3;

layout(location = 0) out vec3 v_normal;
layout(location = 1) out vec3 v_world_pos;
layout(location = 2) out vec4 v_light_space_pos;
layout(location = 3) out vec2 v_uv;
layout(location = 4) out mat3 v_tbn;

const mat4 shadow_bias_matrix = mat4(
0.5, 0.0, 0.0, 0.0,
0.0, -0.5, 0.0, 0.0,
0.0, 0.0, 1.0, 0.0,
0.5, 0.5, 0.0, 1.0);

precise invariant gl_Position;

void main()
{
    mat4 model_matrix = RECONSTRUCT();
    vec4 world_position = model_matrix * vec4(a_position, 1.0);
    gl_Position = camera_ubo.vp * world_position;

    vec3 world_normal = normalize((model_matrix * vec4(a_normal, 0.0)).xyz);
    vec3 world_tangent = normalize((model_matrix * vec4(a_tangent.xyz, 0.0)).xyz);
    vec3 world_bitangent = normalize(cross(world_normal, world_tangent) * a_tangent.w);

    v_normal = world_normal;
    v_world_pos = world_position.xyz;
    v_light_space_pos = shadow_bias_matrix * shadow_ubo.light_vp * world_position;
    v_uv = a_uv;
    v_tbn = mat3(world_tangent, world_bitangent, world_normal);
}