#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(set = 0, binding = 0) uniform CameraUBO
{
  mat4 vp;
};

layout(set = 0, binding = 1, std140) uniform ShadowUniformBufferObject
{
  mat4 light_vp;
  vec4 light_position; // xyz: position, w: unused
  vec4 light_color;    // xyz: color, w: unused
  vec4 _padding_[2];
};

layout(location = 0) out vec3 frag_color;

void main()
{
  gl_Position = vp * vec4(in_position, 1.0);
  frag_color = in_normal;
}
