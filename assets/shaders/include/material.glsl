#ifndef _MATERIAL_GLSL
#define _MATERIAL_GLSL

layout(push_constant) uniform MaterialConstants {
    vec4 albedo;
    float roughness;
    float metallic;
    float ao;
    float emissive_strength;
    vec3 emissive_color;
    float clearcoat;
    float clearcoat_roughness;
    float anisotropy;
    float alpha_cutoff;
    uint flags;
} material;

const uint FLAG_ALPHA_TEST        = 1u << 0;
const uint FLAG_DOUBLE_SIDED      = 1u << 1;
const uint FLAG_EMISSIVE          = 1u << 2;
const uint FLAG_ALBEDO_TEXTURE    = 1u << 3;
const uint FLAG_NORMAL_MAP        = 1u << 4;
const uint FLAG_ROUGHNESS_MAP     = 1u << 5;
const uint FLAG_METALLIC_MAP      = 1u << 6;
const uint FLAG_AO_MAP            = 1u << 7;
const uint FLAG_EMISSIVE_MAP      = 1u << 8;

bool is_alpha_testing()   { return (material.flags & FLAG_ALPHA_TEST) != 0u; }
bool is_emissive()        { return (material.flags & FLAG_EMISSIVE) != 0u; }
bool has_albedo_texture() { return (material.flags & FLAG_ALBEDO_TEXTURE) != 0u; }
bool has_normal_map()     { return (material.flags & FLAG_NORMAL_MAP) != 0u; }
bool has_roughness_map()  { return (material.flags & FLAG_ROUGHNESS_MAP) != 0u; }
bool has_metallic_map()   { return (material.flags & FLAG_METALLIC_MAP) != 0u; }
bool has_ao_map()         { return (material.flags & FLAG_AO_MAP) != 0u; }
bool has_emissive_map()   { return (material.flags & FLAG_EMISSIVE_MAP) != 0u; }

#endif
