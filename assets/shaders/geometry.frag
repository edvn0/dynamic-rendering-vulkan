#version 460

#include "set0.glsl"

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;

layout(location = 0) out vec4 frag_colour;

#define AMBIENT_LIGHT 0.2

void main()
{
  vec3 normal = normalize(v_normal);
  vec3 light_dir = normalize(vec3(shadow_ubo.light_position) - v_world_pos);

  float diffuse = max(dot(normal, light_dir), 0.0);
  vec3 color = vec3(shadow_ubo.light_color) * diffuse + AMBIENT_LIGHT * vec3(shadow_ubo.ambient_color);
  frag_colour = vec4(color, 1.0);
}