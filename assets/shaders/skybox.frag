#version 450

layout(set = 1, binding = 0) uniform samplerCube skybox_sampler;

layout(location = 0) in vec3 inUVW;

layout(location = 0) out vec4 outFragColor;

void main()
{
    outFragColor = texture(skybox_sampler, vec3(inUVW.x, -inUVW.y, inUVW.z));
}