#version 450

#include "set0.glsl"

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(location = 0) out vec4 out_color;

vec3 tone_map_aces(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float luminance(vec3 color)
{
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

vec3 apply_contrast(vec3 color, float contrast)
{
    return mix(vec3(0.5), color, contrast);
}

vec3 adjust_saturation(vec3 color, float saturation)
{
    float lum = luminance(color);
    return mix(vec3(lum), color, saturation);
}

vec3 white_balance(vec3 color, vec3 tint)
{
    return color * tint;
}

vec3 gamma_correct(vec3 color, float gamma)
{
    return pow(color, vec3(1.0 / gamma));
}

layout(push_constant) uniform ColourCorrectionPushConstants
{
    float exposure;
    float contrast;
    float gamma;
    float saturation;
    vec3 tint;
}
pc;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(input_image, 0));
    vec3 hdr = texture(input_image, uv).rgb;

    vec3 color = hdr * pc.exposure;
    color = tone_map_aces(color);
    color = apply_contrast(color, pc.contrast);
    color = adjust_saturation(color, pc.saturation);
    color = white_balance(color, pc.tint);
    color = gamma_correct(color, pc.gamma);

    out_color = vec4(color, 1.0);
}
