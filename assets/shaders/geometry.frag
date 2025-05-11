#version 460

#include "matrix_math.glsl"
#include "set0.glsl"
#include "material.glsl"

layout(set = 1, binding = 0) uniform sampler2D albedo_map;
layout(set = 1, binding = 1) uniform sampler2D normal_map;
layout(set = 1, binding = 2) uniform sampler2D roughness_map;
layout(set = 1, binding = 3) uniform sampler2D metallic_map;
layout(set = 1, binding = 4) uniform sampler2D ao_map;
layout(set = 1, binding = 5) uniform sampler2D emissive_map;

layout(location = 0) in vec3 v_normal;
layout(location = 1) in vec3 v_world_pos;
layout(location = 2) in vec4 v_light_space_pos;
layout(location = 3) in vec2 v_uv;

layout(location = 0) out vec4 frag_colour;

#define AMBIENT_LIGHT 0.03
#define SHADOW_INTENSITY 0.1
#define PI 3.14159265359

const int PCF_SAMPLES = 1;
const int PCF_TOTAL = (PCF_SAMPLES * 2 + 1) * (PCF_SAMPLES * 2 + 1);
const float INVERSE_SHADOW_MAP_SIZE = 1.0 / 2048.0;

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;

    float num = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;

    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return num / denom;
}

float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);

    return ggx1 * ggx2;
}

float calculate_shadow(vec4 light_space_pos)
{
    vec3 proj_coords = light_space_pos.xyz / light_space_pos.w;

    if (proj_coords.x < 0.0 || proj_coords.x > 1.0 ||
        proj_coords.y < 0.0 || proj_coords.y > 1.0 ||
        proj_coords.z < 0.0 || proj_coords.z > 1.0)
        return 1.0;

    float shadow = 0.0;

    for (int x = -PCF_SAMPLES; x <= PCF_SAMPLES; ++x)
    {
        for (int y = -PCF_SAMPLES; y <= PCF_SAMPLES; ++y)
        {
            vec2 offset = vec2(x, y) * INVERSE_SHADOW_MAP_SIZE;
            shadow += texture(shadow_image, vec3(proj_coords.xy + offset, proj_coords.z));
        }
    }

    return shadow / float(PCF_TOTAL);
}

vec3 calculateNormalFromMap(vec2 uv, vec3 normal, vec3 worldPos)
{
    vec3 tangentNormal = texture(normal_map, uv).xyz * 2.0 - 1.0;

    vec3 Q1 = dFdx(worldPos);
    vec3 Q2 = dFdy(worldPos);
    vec2 st1 = dFdx(uv);
    vec2 st2 = dFdy(uv);

    vec3 N = normalize(normal);
    vec3 T = normalize(Q1 * st2.y - Q2 * st1.y);
    vec3 B = -normalize(cross(N, T));
    mat3 TBN = mat3(T, B, N);

    return normalize(TBN * tangentNormal);
}

void main()
{
    vec4 albedo_tex = texture(albedo_map, v_uv);
    vec3 albedo = has_albedo_texture() ? material.albedo.rgb * albedo_tex.rgb : material.albedo.rgb;

    vec4 roughness_sample = texture(roughness_map, v_uv);
    vec4 metallic_sample = texture(metallic_map, v_uv);

    float roughness = has_roughness_map()
                          ? material.roughness * roughness_sample.g
                          : material.roughness;

    float metallic = has_metallic_map()
                         ? material.metallic * metallic_sample.b
                         : material.metallic;

    float ao = has_ao_map()
                   ? material.ao * texture(ao_map, v_uv).r
                   : material.ao;

    vec3 N = normalize(v_normal);
    if (has_normal_map())
    {
        N = calculateNormalFromMap(v_uv, N, v_world_pos);
    }

    vec3 V = normalize(vec3(camera_ubo.camera_position) - v_world_pos);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);

    vec3 Lo = vec3(0.0);
    vec3 L = normalize(vec3(shadow_ubo.light_position) - v_world_pos);
    vec3 H = normalize(V + L);
    float distance = length(vec3(shadow_ubo.light_position) - v_world_pos);
    float attenuation = 1.0 / (distance * distance);
    vec3 radiance = vec3(shadow_ubo.light_color) * attenuation;

    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    vec3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    vec3 kS = F;
    vec3 kD = (1.0 - kS) * (1.0 - metallic);
    float NdotL = max(dot(N, L), 0.0);

    vec3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.001;
    vec3 specular = numerator / denominator;

    Lo += (kD * albedo / PI + specular) * radiance * NdotL;

    float shadow_factor = calculate_shadow(v_light_space_pos);
    vec3 ambient = AMBIENT_LIGHT * albedo * ao * vec3(shadow_ubo.light_color);
    vec3 color = ambient + shadow_factor * Lo;

    if (is_emissive())
    {
        vec3 emissive_tex = has_emissive_map()
                                ? texture(emissive_map, v_uv).rgb
                                : vec3(1.0);
        color += material.emissive_color * material.emissive_strength * emissive_tex;
    }

    float alpha = material.albedo.a;
    if (has_albedo_texture())
    {
        alpha *= albedo_tex.a;
    }

    if (is_alpha_testing() && alpha < material.alpha_cutoff)
    {
        discard;
    }

    frag_colour = vec4(color, alpha);
}
