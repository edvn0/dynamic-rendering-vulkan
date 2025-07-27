#version 460

#include "material.glsl"
#include "matrix_math.glsl"
#include "set0.glsl"

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D normal_map;
layout(set = 1, binding = 2) uniform sampler2D roughness_map;
layout(set = 1, binding = 3) uniform sampler2D metallic_map;
layout(set = 1, binding = 4) uniform sampler2D ao_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

layout(std430, set = 1, binding = 6) restrict buffer light_index_list
{
  uint light_indices[];
};

struct LightGridEntry
{
  uint offset;
  uint count;
  uint pad0;
  uint pad1;
};
layout(std430, set = 1, binding = 7) restrict buffer light_grid_buffer
{
  LightGridEntry tile_light_grids[];
};

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;
layout(location = 3) in vec2 v_uv;
layout(location = 4) in mat3 v_tbn;

layout(location = 0) out vec4 frag_colour;

#define AMBIENT_LIGHT 0.3
const int PCF_SAMPLES = 1;
const int PCF_TOTAL = (PCF_SAMPLES * 2 + 1) * (PCF_SAMPLES * 2 + 1);
const float INVERSE_SHADOW_MAP_SIZE = 1.0 / 2048.0;

vec3 visualize_normals(vec3 normal)
{
  return normal * 0.5 + 0.5;
}

float calculate_shadow(vec4 light_space_pos)
{
  vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
  if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 ||
      proj_coords.y > 1.0 || proj_coords.z < 0.0 || proj_coords.z > 1.0)
    return 0.1;

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
  return shadow / float(PCF_TOTAL);
}

vec3 calculate_normal_from_map(vec2 uv, mat3 tbn)
{
  vec3 tangent_normal = texture(normal_map, uv).xyz;
  tangent_normal = tangent_normal * 2.0 - 1.0;
  return normalize(tbn * tangent_normal);
}

void main()
{
  vec4 albedo_tex = texture(albedo_map, v_uv);
  vec3 albedo = has_albedo_texture() ? material.albedo.rgb * albedo_tex.rgb
                                     : material.albedo.rgb;

  float roughness = has_roughness_map()
                        ? material.roughness * texture(roughness_map, v_uv).g
                        : material.roughness;

  float metallic = has_metallic_map()
                       ? material.metallic * texture(metallic_map, v_uv).b
                       : material.metallic;

  float ao = has_ao_map() ? material.ao * texture(ao_map, v_uv).r : material.ao;

  vec3 n = normalize(v_normal);
  if (has_normal_map())
  {
    n = calculate_normal_from_map(v_uv, v_tbn);
  }

  // Naive point light iteration
  vec3 lit_color = vec3(0.0);
  for (uint i = 0; i < point_light_buffer.light_count; ++i)
  {
    PointLight light = point_light_buffer.lights[i];
    vec3 to_light = light.position - v_world_pos;
    float dist = length(to_light);
    vec3 dir = to_light / dist;

    float attenuation = 1.0 / (dist * dist + 1.0);
    float radius_fade = clamp(1.0 - dist / light.radius, 0.0, 1.0);
    float ndotl = max(dot(n, dir), 0.0);

    vec3 irradiance = light.color * light.intensity * ndotl * attenuation * radius_fade;
    lit_color += irradiance;
  }

  vec3 ambient = AMBIENT_LIGHT * albedo * ao;
  vec3 color = ambient + lit_color * albedo;

  // Optional debug outputs
  // color = albedo;
  // color = vec3(roughness);
  // color = vec3(metallic);
  // color = vec3(ao);
  // color = visualize_normals(n);
  // color = vec3(calculate_shadow(v_light_space_pos));

  if (is_emissive())
  {
    vec3 emissive_tex =
        has_emissive_map() ? texture(emissive_map, v_uv).rgb : vec3(1.0);
    color +=
        material.emissive_color * material.emissive_strength * emissive_tex;
  }

  float alpha = material.albedo.a;
  if (has_albedo_texture())
    alpha *= albedo_tex.a;

  if (is_alpha_testing() && alpha < material.alpha_cutoff)
    discard;

  frag_colour = vec4(color, 1.0);
}
