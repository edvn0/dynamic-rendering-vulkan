#version 460

layout(location = 0) in vec3 a_normal;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 normal = normalize(a_normal);
    fragColor = vec4(normal, 1.0);
    fragColor.rgb = (fragColor.rgb + 1.0) / 2.0;
    fragColor.a = 1.0;
}