#version 460

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;

layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 normal = normalize(v_normal);
    vec3 light_pos = vec3(40.0, 40.0, 40.0);
    vec3 light_color = vec3(1.0, 0.5, 0.0); // orange
    vec3 light_dir = normalize(light_pos - v_world_pos);

    float diffuse = max(dot(normal, light_dir), 0.0);
    vec3 color = diffuse * light_color;

    fragColor = vec4(color, 1.0);
}