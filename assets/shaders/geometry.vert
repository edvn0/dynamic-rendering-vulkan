#version 460

layout(location = 0) in vec3 a_position;
layout(location = 1) in vec3 a_normal;
layout(location = 2) in vec2 a_uv;
layout(location = 3) in vec4 row_0;
layout(location = 4) in vec4 row_1;
layout(location = 5) in vec4 row_2;

layout(location = 0) out vec3 v_normal;

void main()
{
  // Generate the model matrix from the rows
  mat4 model_matrix = mat4(row_0, row_1, row_2, vec4(0.0, 0.0, 0.0, 1.0));

  gl_Position = model_matrix * vec4(a_position, 1.0);
  v_normal = a_normal;
}