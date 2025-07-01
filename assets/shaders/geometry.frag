#version 460

#include "material.glsl"
#include "matrix_math.glsl"
#include "set0.glsl"
#include "tiling_constants.glsl"

#extension GL_EXT_debug_printf : enable

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D normal_map;
layout(set = 1, binding = 2) uniform sampler2D roughness_map;
layout(set = 1, binding = 3) uniform sampler2D metallic_map;
layout(set = 1, binding = 4) uniform sampler2D ao_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

layout(std430, set = 1, binding = 6) restrict buffer LightIndexList
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
layout(std430, set = 1, binding = 7) restrict buffer LightGridData
{
  LightGridEntry tile_light_grids[];
};

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;
layout(location = 3) in vec2 v_uv;
layout(location = 4) in mat3 v_tbn;

layout(location = 0) out vec4 frag_colour;

#define AMBIENT_LIGHT 0.03
#define PI 3.14159265359

const int PCF_SAMPLES = 1;
const int PCF_TOTAL = (PCF_SAMPLES * 2 + 1) * (PCF_SAMPLES * 2 + 1);
const float INVERSE_SHADOW_MAP_SIZE = 1.0 / 2048.0;

vec3 fresnel_schlick(float cos_theta, vec3 f0)
{
  return f0 + (1.0 - f0) * pow(clamp(1.0 - cos_theta, 0.0, 1.0), 5.0);
}

float distribution_ggx(vec3 n, vec3 h, float roughness)
{
  float a = roughness * roughness;
  float a2 = a * a;
  float ndoth = max(dot(n, h), 0.0);
  float ndoth2 = ndoth * ndoth;

  float denom = (ndoth2 * (a2 - 1.0) + 1.0);
  return a2 / (PI * denom * denom);
}

float geometry_schlick_ggx(float ndotv, float roughness)
{
  float r = (roughness + 1.0);
  float k = (r * r) / 8.0;
  return ndotv / (ndotv * (1.0 - k) + k);
}

float geometry_smith(vec3 n, vec3 v, vec3 l, float roughness)
{
  float ndotv = max(dot(n, v), 0.0);
  float ndotl = max(dot(n, l), 0.0);
  float ggx1 = geometry_schlick_ggx(ndotl, roughness);
  float ggx2 = geometry_schlick_ggx(ndotv, roughness);
  return ggx1 * ggx2;
}

float calculate_shadow(vec4 light_space_pos)
{
  vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
  if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 ||
      proj_coords.y > 1.0 || proj_coords.z < 0.0 || proj_coords.z > 1.0)
  {
    return 1.0;
  }

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
  vec3 tangent_normal = texture(normal_map, uv).xyz * 2.0 - 1.0;
  return normalize(tbn * tangent_normal);
}

float linear_depth(uint slice, uint num_slices, float near_z, float far_z)
{
  float slice_z = float(slice) / float(num_slices);
  return near_z * pow(far_z / near_z, slice_z);
}

// Get the Z-slice index for current fragment
uint get_z_slice(float view_space_z, float near_plane, float far_plane)
{
  // Convert view space Z to slice index using same distribution as compute
  // shader
  float normalized_z = (-view_space_z - near_plane) / (far_plane - near_plane);
  normalized_z = clamp(normalized_z, 0.0, 1.0);

  // Use logarithmic distribution to match compute shader
  float slice_float = log(normalized_z * (far_plane / near_plane - 1.0) + 1.0) /
                      log(far_plane / near_plane) * float(NUM_Z_SLICES);

  return min(uint(slice_float), NUM_Z_SLICES - 1u);
}

// Get 3D tile index (matching compute shader logic)
uint get_tile_index_3d(uvec3 tile_coord, uvec3 tile_grid_size)
{
  return tile_coord.z * tile_grid_size.x * tile_grid_size.y +
         tile_coord.y * tile_grid_size.x + tile_coord.x;
}

void main()
{
  vec4 albedo_tex = texture(albedo_map, v_uv);
  vec3 albedo = has_albedo_texture() ? material.albedo.rgb * albedo_tex.rgb
                                     : material.albedo.rgb;

  if (albedo_tex.a < 0.9)
    discard;

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

  vec3 v = -normalize(v_world_pos - vec3(camera_ubo.camera_position));
  vec3 l = normalize(v_world_pos - vec3(shadow_ubo.light_position));
  vec3 h = normalize(v + l);

  float distance = length(vec3(shadow_ubo.light_position) - v_world_pos);
  float attenuation = 1.0 / (distance + 1.0);
  vec3 light_color = vec3(shadow_ubo.light_color);
  vec3 radiance = light_color * attenuation;

  vec3 f0 = mix(vec3(0.04), albedo, metallic);
  vec3 f = fresnel_schlick(max(dot(h, v), 0.0), f0);
  float ndf = distribution_ggx(n, h, roughness);
  float g = geometry_smith(n, v, l, roughness);

  float ndotl = max(dot(n, l), 0.0);
  float ndotv = max(dot(n, v), 0.0);
  float denominator = 4.0 * ndotv * ndotl + 0.001;

  vec3 specular = (ndf * g * f) / denominator;
  vec3 kd = (1.0 - f) * (1.0 - metallic);

  vec3 diffuse = kd * albedo / PI;
  vec3 lighting = (diffuse + specular) * radiance * ndotl;

  float shadow = 1.0F - calculate_shadow(v_light_space_pos);
  vec3 ambient = vec3(shadow_ubo.ambient_color.rgb) *
                 AMBIENT_LIGHT * albedo * ao;

  // --- Forward+ tiled lights integration ---

  uint tile_x = uint(gl_FragCoord.x) / TILE_SIZE;
  uint tile_y = uint(gl_FragCoord.y) / TILE_SIZE;

  // Get view space Z for this fragment
  vec4 view_pos = camera_ubo.view * vec4(v_world_pos, 1.0);
  float view_z = view_pos.z; // Negative in right-handed view space

  // Calculate Z-slice
  float near_plane = camera_ubo.screen_size_near_far.z;
  float far_plane = camera_ubo.screen_size_near_far.w;
  uint tile_z = get_z_slice(view_z, near_plane, far_plane);

  // Calculate tile grid dimensions (same as compute shader)
  uvec2 tile_count_xy =
      uvec2(ceil(camera_ubo.screen_size_near_far.xy / float(TILE_SIZE)));
  uvec3 tile_grid_size = uvec3(tile_count_xy, NUM_Z_SLICES);

  // Get 3D tile index
  uvec3 tile_coord = uvec3(tile_x, tile_y, tile_z);
  uint tile_index = get_tile_index_3d(tile_coord, tile_grid_size);

  // Bounds check
  if (tile_coord.x >= tile_grid_size.x || tile_coord.y >= tile_grid_size.y ||
      tile_coord.z >= tile_grid_size.z)
  {
    // Fragment outside valid tile range - use fallback lighting or discard
    return;
  }

  // Get light list for this tile
  LightGridEntry tile_entry = tile_light_grids[tile_index];
  uint offset = tile_entry.offset;
  uint count = tile_entry.count;

  // Clamp count to prevent buffer overruns
  count = min(count, MAX_LIGHTS_PER_TILE);

  // Process all lights in this tile
  for (uint i = 0u; i < count; ++i)
  {
    uint light_index = light_indices[offset + i];

    // Safety check
    if (light_index >= point_light_buffer.light_count)
    {
      continue;
    }

    PointLight light = point_light_buffer.lights[light_index];

    // Calculate lighting contribution
    vec3 light_to_frag = v_world_pos - light.position;
    float dist = length(light_to_frag);

    // Early exit if fragment is outside light radius
    if (dist >= light.radius)
    {
      continue;
    }

    vec3 light_dir = light_to_frag / dist; // Normalize and flip direction

    // Attenuation calculation
    float att =
        light.intensity / (dist * dist + 1.0); // +1.0 prevents divide by zero

    // Apply light radius falloff
    float radius_falloff = 1.0 - pow(dist / light.radius, 4.0);
    radius_falloff = max(radius_falloff, 0.0);
    radius_falloff = radius_falloff * radius_falloff;
    att *= radius_falloff;

    vec3 radiance = light.color * att;

    // PBR lighting calculation
    vec3 half_vec = normalize(-v + light_dir);
    float ndotl = max(dot(n, light_dir), 0.0);

    if (ndotl > 0.0)
    {
      vec3 fresnel = fresnel_schlick(max(dot(half_vec, v), 0.0), f0);
      float ndf = distribution_ggx(n, half_vec, roughness);
      float geometry = geometry_smith(n, v, light_dir, roughness);

      vec3 specular = (ndf * geometry * fresnel) /
                      (4.0 * max(dot(n, v), 0.0) * ndotl + 0.001);

      vec3 kd = (1.0 - fresnel) * (1.0 - metallic);
      vec3 diffuse = kd * albedo / PI;

      lighting += (diffuse + specular) * radiance * ndotl;
    }
  }

  vec3 emissive_tex =
      has_emissive_map() ? texture(emissive_map, v_uv).rgb : vec3(1.0);
  vec3 emission =
      material.emissive_color * material.emissive_strength * emissive_tex;

  vec3 color = ambient + shadow * lighting + emission;
  frag_colour = vec4(color, albedo_tex.a);
}
