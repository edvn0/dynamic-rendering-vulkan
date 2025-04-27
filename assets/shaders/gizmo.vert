#version 460

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(location = 0) out vec3 out_color;

layout(push_constant) uniform PushConstants { mat4 rotation_matrix; }
pc;

void main() {
  vec4 pos = pc.rotation_matrix * vec4(in_position, 0.0);

  pos.xy *= 0.3;
  pos.x += 0.8;
  pos.y += 0.8;

  gl_Position = vec4(pos.xy, 0.0, 1.0);

  out_color = in_color;
}
