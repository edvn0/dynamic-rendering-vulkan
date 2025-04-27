#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 out_color;

layout(push_constant) uniform PushConstants { mat4 rotation_matrix; }
pc;

layout(set = 0, binding = 1, std140) uniform ShadowUniformBufferObject
{
  mat4 light_vp;
  vec4 light_position; // xyz: position, w: unused
  vec4 light_color;    // xyz: color, w: unused
  vec4 _padding_[2];
};

void main()
{
  vec4 pos = pc.rotation_matrix * vec4(in_position, 0.0);

  // Draw small in top-right of screen
  pos.xy *= 0.3; // shrink the gizmo size
  pos.x += 0.8;  // move to top-right
  pos.y += 0.8;

  gl_Position = vec4(pos.xy, 0.0, 1.0);

  out_color = in_color;
}
