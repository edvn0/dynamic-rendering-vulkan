#version 450

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(push_constant, std430) uniform Push {
    vec2 framebuffer_size;
};

void main()
{
    vec2 uv = gl_FragCoord.xy / framebuffer_size;

    out_color = texture(input_image, uv);
}