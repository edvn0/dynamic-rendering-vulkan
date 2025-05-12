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

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(input_image, 0));
    vec3 hdr = texture(input_image, uv).rgb;

    float exposure = 30.0; // make this uniform later
    vec3 mapped = tone_map_aces(hdr * exposure);

    out_color = vec4(hdr, 1.0);
}
