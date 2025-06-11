#version 450

layout(location = 0) out vec4 out_color;

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(push_constant, std430) uniform Push {
    vec2 framebuffer_size;// size of the destination mip
};

void main()
{
    vec2 texel_size = 1.0 / framebuffer_size;
    vec2 uv = gl_FragCoord.xy * texel_size;

    // 4-tap Gaussian-ish box blur
    vec4 color =
    texture(input_image, uv + texel_size * vec2(-0.5, -0.5)) +
    texture(input_image, uv + texel_size * vec2(0.5, -0.5)) +
    texture(input_image, uv + texel_size * vec2(-0.5, 0.5)) +
    texture(input_image, uv + texel_size * vec2(0.5, 0.5));

    out_color = color * 0.25;
}
