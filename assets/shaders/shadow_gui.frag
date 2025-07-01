#version 450

layout(set = 1, binding = 0) uniform sampler2D u_input_shadow;

layout(location = 0) out vec4 out_color;

void main()
{
    vec2 uv = gl_FragCoord.xy / vec2(textureSize(u_input_shadow, 0));
    float depth = texture(u_input_shadow, uv).r;

    float grayscale = pow(depth, 1.0 / 2);
    out_color = vec4(vec3(grayscale), 1.0F);
}
