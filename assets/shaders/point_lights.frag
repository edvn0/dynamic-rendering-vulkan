#version 460

#include "material.glsl"
#include "matrix_math.glsl"
#include "set0.glsl"

// Textures
layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D normal_map;
layout(set = 1, binding = 2) uniform sampler2D roughness_map;
layout(set = 1, binding = 3) uniform sampler2D metallic_map;
layout(set = 1, binding = 4) uniform sampler2D ao_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

// Inputs
layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;
layout(location = 3) in vec2 v_uv;
layout(location = 4) in flat uint v_instance_index;
layout(location = 5) in mat3 v_tbn;

// Outputs
layout(location = 0) out vec4 frag_colour;

void main() {
  // Alpha test
  float alpha = material.albedo.a;
  if (has_albedo_texture())
    alpha *= texture(albedo_map, v_uv).a;

  if (is_alpha_testing() && alpha < material.alpha_cutoff)
    discard;

  // Emissive output only
  vec3 emissive = vec3(0.0);
  if (is_emissive()) {
    vec3 emissive_tex =
        has_emissive_map() ? texture(emissive_map, v_uv).rgb : vec3(1.0);

    emissive =
        material.emissive_color * material.emissive_strength * emissive_tex;
  }

  frag_colour = vec4(emissive, 1.0);
}
