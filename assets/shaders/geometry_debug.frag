#version 460

#include "material.glsl"
#include "matrix_math.glsl"
#include "set0.glsl"

// Keep all the texture samplers for debugging
layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D normal_map;
layout(set = 1, binding = 2) uniform sampler2D roughness_map;
layout(set = 1, binding = 3) uniform sampler2D metallic_map;
layout(set = 1, binding = 4) uniform sampler2D ao_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

layout(std430, set = 1, binding = 6) restrict buffer LightIndexList {
  uint light_indices[];
};

struct LightGridEntry {
  uint offset;
  uint count;
  uint pad0;
  uint pad1;
};
layout(std430, set = 1, binding = 7) restrict buffer LightGridData {
  LightGridEntry tile_light_grids[];
};

// Input variables
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;
layout(location = 3) in vec2 v_uv;
layout(location = 4) in mat3 v_tbn;

// Output
layout(location = 0) out vec4 frag_colour;

// Lighting constants
#define AMBIENT_LIGHT 0.3

// PCF Shadow mapping parameters (same as original)
const int PCF_SAMPLES = 1;
const int PCF_TOTAL = (PCF_SAMPLES * 2 + 1) * (PCF_SAMPLES * 2 + 1);
const float INVERSE_SHADOW_MAP_SIZE = 1.0 / 2048.0;

// Simple function to visualize normals
vec3 visualize_normals(vec3 normal) {
  return normal * 0.5 + 0.5; // Convert from [-1,1] to [0,1] range
}

// Shadow calculation function (kept from original)
float calculate_shadow(vec4 light_space_pos) {
  vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;
  if (proj_coords.x < 0.0 || proj_coords.x > 1.0 || proj_coords.y < 0.0 ||
      proj_coords.y > 1.0 || proj_coords.z < 0.0 || proj_coords.z > 1.0)
    return 0.1;

  float shadow = 0.0;
  for (int x = -PCF_SAMPLES; x <= PCF_SAMPLES; ++x) {
    for (int y = -PCF_SAMPLES; y <= PCF_SAMPLES; ++y) {
      vec2 offset = vec2(x, y) * INVERSE_SHADOW_MAP_SIZE;
      shadow +=
          texture(shadow_image, vec3(proj_coords.xy + offset, proj_coords.z));
    }
  }
  return shadow / float(PCF_TOTAL);
}

vec3 calculate_normal_from_map(vec2 uv, mat3 tbn) {
  vec3 tangent_normal = texture(normal_map, uv).xyz;
  tangent_normal = tangent_normal * 2.0 - 1.0;
  return normalize(tbn * tangent_normal);
}

void main() {
  // Basic texture sampling (keeping all from original)
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

  // Get normal but don't do complex calculations
  vec3 n = normalize(v_normal);
  if (has_normal_map()) {
    n = calculate_normal_from_map(v_uv, v_tbn);
  }

  // Basic lighting direction (simplified)
  vec3 light_dir = normalize(vec3(shadow_ubo.light_position) - v_world_pos);
  float ndotl = max(dot(n, light_dir), 0.0);

  // Calculate shadow same as original shader
  float shadow = calculate_shadow(v_light_space_pos);

  // Basic lighting calculation (no PBR)
  vec3 diffuse = albedo * ndotl;
  vec3 ambient = AMBIENT_LIGHT * albedo * ao;

  // Apply shadow to the diffuse component
  // vec3 color = ambient + shadow * diffuse; // Default: simple lighting with
  // shadows

  // DEBUGGING OPTIONS - Uncomment one of these to debug specific texture or
  // component
  // vec3 color = albedo; // Show just albedo
  // vec3 color = vec3(roughness); // Show just roughness
  // vec3 color = vec3(metallic); // Show just metallicB
  // vec3 color = vec3(ao);                      // Show just ambient occlusion
  vec3 color = visualize_normals(n); // Visualize normals
  // vec3 color = vec3(shadow); // Visualize shadow map values

  // Add emissive if needed
  if (is_emissive()) {
    vec3 emissive_tex =
        has_emissive_map() ? texture(emissive_map, v_uv).rgb : vec3(1.0);
    color +=
        material.emissive_color * material.emissive_strength * emissive_tex;
  }

  // Handle alpha testing
  float alpha = material.albedo.a;
  if (has_albedo_texture())
    alpha *= albedo_tex.a;

  if (is_alpha_testing() && alpha < material.alpha_cutoff)
    discard;

  frag_colour = vec4(color, 1.0);
}