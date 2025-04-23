#version 460

layout(location = 0) in vec3 a_normal;

layout(location = 0) out vec4 fragColor;

void main() { fragColor = vec4(a_normal, 1.0); }