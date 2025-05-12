#version 460

layout(set = 1, binding = 0) uniform sampler2D skybox_input;
layout(set = 1, binding = 1) uniform sampler2D geometry_input;

layout(location = 0) out vec4 out_color;

void main()
{
  vec2 uv = gl_FragCoord.xy / vec2(textureSize(skybox_input, 0));
  vec4 skybox_color = texture(skybox_input, uv);
  vec4 geometry_color = texture(geometry_input, uv);
  out_color = mix(skybox_color, geometry_color, geometry_color.a);
}
