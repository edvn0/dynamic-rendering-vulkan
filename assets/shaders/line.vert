#version 460
#extension GL_GOOGLE_include_directive : require

#include "set0.glsl"

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_normal;
layout(location = 2) in vec2 in_uv;

layout(location = 0) out vec3 frag_color;

void main() {
  gl_Position = camera_ubo.vp * vec4(in_position, 1.0);
  frag_color = in_normal;
}
