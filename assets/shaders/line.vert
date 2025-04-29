#version 460

#include "set0.glsl"

layout(location = 0) in vec3 instance_start;
layout(location = 1) in float instance_width;
layout(location = 2) in vec3 instance_end;
layout(location = 3) in uint instance_packed_color;

layout(location = 0) out vec4 frag_color;

void main()
{
  // camera right/up vectors
  vec3 cam_right = vec3(camera_ubo.vp[0][0], camera_ubo.vp[1][0], camera_ubo.vp[2][0]);
  vec3 cam_up = vec3(camera_ubo.vp[0][1], camera_ubo.vp[1][1], camera_ubo.vp[2][1]);

  // pick start or end per instance vertex index
  bool is_end = bool((gl_VertexIndex >> 1) & 1);
  vec3 center = mix(instance_start, instance_end, is_end ? 1.0 : 0.0);

  // screen‐aligned quad offset
  vec3 line_dir = normalize(instance_end - instance_start);
  vec3 side = normalize(cross(cam_up, line_dir));
  float sign = (gl_VertexIndex & 1) == 0 ? 1.0 : -1.0;
  vec3 offset = side * instance_width * 0.5 * sign;

  vec3 position = center + offset;
  gl_Position = camera_ubo.vp * vec4(position, 1.0);

  frag_color = unpackUnorm4x8(instance_packed_color);
}
