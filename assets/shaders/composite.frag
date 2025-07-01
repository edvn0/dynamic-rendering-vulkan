#version 460

layout(set = 1, binding = 0) uniform sampler2D skybox_input;
layout(set = 1, binding = 1) uniform sampler2D geometry_input;
layout(set = 1, binding = 2, rgba32f) readonly uniform image2D bloom_input;

layout(location = 0) out vec4 out_color;

layout(push_constant) uniform BloomStrength { float bloom_strength; }
pc;

void main() {
  ivec2 framebuffer_size = textureSize(skybox_input, 0);
  ivec2 bloom_size = imageSize(bloom_input);

  // Compute normalized UV in [0,1] relative to framebuffer
  vec2 uv = gl_FragCoord.xy / vec2(framebuffer_size);

  // Convert uv to bloom texture coords using normalized coordinates
  // Using floor to avoid rounding issues and clamp to valid range
  ivec2 bloom_pixel =
      clamp(ivec2(floor(uv * vec2(bloom_size))), ivec2(0), bloom_size - 1);

  vec4 bloom_sample = imageLoad(bloom_input, bloom_pixel);
  vec4 bloom_color = isnan(bloom_sample.x) ? vec4(0) : bloom_sample;
  vec4 skybox_color = texture(skybox_input, uv);
  vec4 geometry_color = texture(geometry_input, uv);
  vec4 lit_color = mix(skybox_color, geometry_color, geometry_color.a);

  out_color = lit_color + pc.bloom_strength * bloom_color;
}
