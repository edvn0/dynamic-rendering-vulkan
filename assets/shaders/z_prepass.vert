#version 460
#extension GL_GOOGLE_include_directive : require

#include "matrix_math.glsl"
#include "set0.glsl"

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 a_model_matrix_row0;
layout(location = 4) in vec4 a_model_matrix_row1;
layout(location = 5) in vec4 a_model_matrix_row2;
layout(location = 6) in vec4 a_model_matrix_row3;

void main()
{
  mat4 model_matrix = RECONSTRUCT();

  vec4 world_position = model_matrix * vec4(a_position, 1.0);
  gl_Position = camera_ubo.inverse_vp * world_position;
}
