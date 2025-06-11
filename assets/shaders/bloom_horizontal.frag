#version 450

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(push_constant, std430) uniform Push {
    vec2 framebuffer_size;// size of the destination mip
};

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(input_image, 0));
    vec2 texel = 1.0 / vec2(textureSize(input_image, 0));

    out_color =
    0.2270270270 * texture(input_image, uv) +
    0.1945945946 * texture(input_image, uv + vec2(texel.x, 0.0)) +
    0.1945945946 * texture(input_image, uv - vec2(texel.x, 0.0)) +
    0.1216216216 * texture(input_image, uv + vec2(2.0 * texel.x, 0.0)) +
    0.1216216216 * texture(input_image, uv - vec2(2.0 * texel.x, 0.0)) +
    0.0540540541 * texture(input_image, uv + vec2(3.0 * texel.x, 0.0)) +
    0.0540540541 * texture(input_image, uv - vec2(3.0 * texel.x, 0.0));
}
