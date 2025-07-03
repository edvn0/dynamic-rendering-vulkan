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
layout(location = 3) in vec2 v_uv;
layout(location = 4) in flat uint v_instance_index;

// Outputs
layout(location = 0) out vec4 frag_colour;

void main()
{
  // Bounds check for light index
  if (v_instance_index >= point_light_buffer.light_count)
    discard;

  PointLight light_data = point_light_buffer.lights[v_instance_index];

  // Alpha test (if needed for light geometry)
  float alpha = material.albedo.a;
  if (has_albedo_texture())
    alpha *= texture(albedo_map, v_uv).a;
  if (is_alpha_testing() && alpha < material.alpha_cutoff)
    discard;

  // Calculate emissive output from point light data
  vec3 emissive_color = light_data.color * light_data.intensity;

  // Optional: Apply distance-based falloff for visual effect
  // You might want to use world position and light position for this
  // float distance_factor = 1.0; // Implement based on your needs

  // Optional: Modulate with emissive texture if available
  if (has_emissive_map())
  {
    vec3 emissive_tex = texture(emissive_map, v_uv).rgb;
    emissive_color *= emissive_tex;
  }

  // Optional: Add material emissive if you want to combine both
  if (is_emissive())
  {
    vec3 material_emissive =
        material.emissive_color * material.emissive_strength;
    if (has_emissive_map())
      material_emissive *= texture(emissive_map, v_uv).rgb;
    emissive_color += material_emissive;
  }

  frag_colour = vec4(emissive_color, alpha);
}