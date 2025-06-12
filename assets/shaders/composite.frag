#version 460

layout(set = 1, binding = 0) uniform sampler2D skybox_input;
layout(set = 1, binding = 1) uniform sampler2D geometry_input;
layout(set = 1, binding = 2, rgba32f) readonly uniform image2D bloom_input;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(skybox_input, 0));
    ivec2 pixel_coord = ivec2(gl_FragCoord.xy);

    vec4 skybox_color = texture(skybox_input, uv);
    vec4 geometry_color = texture(geometry_input, uv);
    vec4 bloom_color = imageLoad(bloom_input, pixel_coord);

    vec4 lit_color = mix(skybox_color, geometry_color, geometry_color.a);

    float bloom_strength = 20;
    out_color = lit_color + bloom_strength * bloom_color;
}
