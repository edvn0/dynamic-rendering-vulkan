#version 450

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(location = 0) out vec4 out_color;

layout(push_constant, std430) uniform Push {
    vec2 framebuffer_size;// 8 bytes, offset 8 (requires 8-byte alignment)
};

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(input_image, 0));
    vec2 texel = 1.0 / vec2(textureSize(input_image, 0));

    out_color =
    0.2270270270 * texture(input_image, uv) +
    0.1945945946 * texture(input_image, uv + vec2(0.0, texel.y)) +
    0.1945945946 * texture(input_image, uv - vec2(0.0, texel.y)) +
    0.1216216216 * texture(input_image, uv + vec2(0.0, 2.0 * texel.y)) +
    0.1216216216 * texture(input_image, uv - vec2(0.0, 2.0 * texel.y)) +
    0.0540540541 * texture(input_image, uv + vec2(0.0, 3.0 * texel.y)) +
    0.0540540541 * texture(input_image, uv - vec2(0.0, 3.0 * texel.y));
}
