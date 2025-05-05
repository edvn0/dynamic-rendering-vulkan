#version 450

#include "set0.glsl"

layout(set = 1, binding = 0) uniform sampler2D input_image;

layout(location = 0) out vec4 out_color;

vec3 apply_gamma(vec3 linear_rgb) { return pow(linear_rgb, vec3(1.0 / 2.2)); }

vec3 tone_map_aces(vec3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main()
{
    vec3 hdr_color =
        texture(input_image, gl_FragCoord.xy / textureSize(input_image, 0)).rgb;
    vec3 tone_mapped = tone_map_aces(hdr_color);
    out_color = vec4(apply_gamma(tone_mapped), 1.0);
}
