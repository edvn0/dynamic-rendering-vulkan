#version 460

#include "set0.glsl"
#include "matrix_math.glsl"

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;

layout(location = 0) out vec4 frag_colour;

#define AMBIENT_LIGHT 0.1
#define SHADOW_INTENSITY 0.1

const int PCF_SAMPLES = 1;
const int PCF_TOTAL = (PCF_SAMPLES * 2 + 1) * (PCF_SAMPLES * 2 + 1);

const int SHADOW_MAP_SIZE = 2048;
const float INVERSE_SHADOW_MAP_SIZE = 0.00048828125;

float calculate_shadow(vec4 light_space_pos)
{
  vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
  if (proj_coords.z > 1.0)
    return 1.0;

  proj_coords.y = -proj_coords.y;

  float shadow = 0.0;

  for (int x = -PCF_SAMPLES; x <= PCF_SAMPLES; ++x)
  {
    for (int y = -PCF_SAMPLES; y <= PCF_SAMPLES; ++y)
    {
      vec2 offset = vec2(x, y) * INVERSE_SHADOW_MAP_SIZE;
      shadow +=
          texture(shadow_image, vec3(proj_coords.xy + offset, proj_coords.z));
    }
  }

  float total_samples = PCF_TOTAL;
  return shadow / total_samples;
}

void main()
{
  vec3 normal = v_normal;
  vec3 light_dir = normalize(vec3(shadow_ubo.light_position) - v_world_pos);

  float diffuse = max(dot(normal, light_dir), 0.0);
  float shadow_factor = calculate_shadow(v_light_space_pos);

  vec3 light_contrib = diffuse * vec3(shadow_ubo.light_color);
  vec3 ambient = AMBIENT_LIGHT * vec3(shadow_ubo.ambient_color);
  vec3 color = light_contrib + ambient;

  frag_colour = vec4(shadow_factor, 0.0, 0.0, 1.0);
}
