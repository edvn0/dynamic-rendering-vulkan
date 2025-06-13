#version 460

layout(set = 1, binding = 0) uniform sampler2D skybox_input;
layout(set = 1, binding = 1) uniform sampler2D geometry_input;
layout(set = 1, binding = 2, rgba32f) readonly uniform image2D bloom_input;

layout(location = 0) out vec4 out_color;

void main() {
  ivec2 framebuffer_size = textureSize(skybox_input, 0);
  ivec2 bloom_size = imageSize(bloom_input);

  vec2 uv = gl_FragCoord.xy / vec2(framebuffer_size);
  ivec2 bloom_pixel = ivec2(uv * vec2(bloom_size));

  vec4 bloom_color =
      imageLoad(bloom_input, clamp(bloom_pixel, ivec2(0), bloom_size - 1));

  vec4 skybox_color = texture(skybox_input, uv);
  vec4 geometry_color = texture(geometry_input, uv);
  vec4 lit_color = mix(skybox_color, geometry_color, geometry_color.a);

  float bloom_strength = 20.0;
  out_color = lit_color + bloom_strength * bloom_color;
}
