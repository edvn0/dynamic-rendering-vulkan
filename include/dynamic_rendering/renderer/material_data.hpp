#pragma once

#include <glm/glm.hpp>

struct MaterialData
{
  glm::vec4 albedo{ 1.0f, 1.0f, 1.0f, 1.0f };
  float roughness = 0.5f;
  float metallic = 0.0f;
  float ao = 1.0f;

  float emissive_strength = 0.0f;
  glm::vec3 emissive_color{ 0.0f, 0.0f, 0.0f };
  float clearcoat = 0.0f;

  float clearcoat_roughness = 0.0f;
  float anisotropy = 0.0f;
  float alpha_cutoff = 0.5f;
  std::uint32_t flags = 0;

  explicit(false)
    MaterialData(const glm::vec4& color = { 1.0f, 1.0f, 1.0f, 1.0f },
                 const float rough = 0.5f,
                 const float metal = 0.0f)
    : albedo(color)
    , roughness(rough)
    , metallic(metal)
  {
  }

  static constexpr std::uint32_t FLAG_ALPHA_TEST = 1 << 0;
  static constexpr std::uint32_t FLAG_DOUBLE_SIDED = 1 << 1;
  static constexpr std::uint32_t FLAG_EMISSIVE = 1 << 2;
  static constexpr std::uint32_t FLAG_ALBEDO_TEXTURE = 1 << 3;
  static constexpr std::uint32_t FLAG_NORMAL_MAP = 1 << 4;
  static constexpr std::uint32_t FLAG_ROUGHNESS_MAP = 1 << 5;
  static constexpr std::uint32_t FLAG_METALLIC_MAP = 1 << 6;
  static constexpr std::uint32_t FLAG_AO_MAP = 1 << 7;
  static constexpr std::uint32_t FLAG_EMISSIVE_MAP = 1 << 8;

  constexpr auto is_alpha_testing() const -> bool
  {
    return flags & FLAG_ALPHA_TEST;
  }
  constexpr auto is_emissive() const -> bool { return flags & FLAG_EMISSIVE; }
  constexpr auto has_albedo_texture() const -> bool
  {
    return flags & FLAG_ALBEDO_TEXTURE;
  }
  constexpr auto has_normal_map() const -> bool
  {
    return flags & FLAG_NORMAL_MAP;
  }
  constexpr auto has_roughness_map() const -> bool
  {
    return flags & FLAG_ROUGHNESS_MAP;
  }
  constexpr auto has_metallic_map() const -> bool
  {
    return flags & FLAG_METALLIC_MAP;
  }
  constexpr auto has_ao_map() const -> bool { return flags & FLAG_AO_MAP; }
  constexpr auto has_emissive_map() const -> bool
  {
    return flags & FLAG_EMISSIVE_MAP;
  }

  constexpr auto set_alpha_testing(const bool val) -> void
  {
    if (val)
      flags |= FLAG_ALPHA_TEST;
    else
      flags &= ~FLAG_ALPHA_TEST;
  }

  constexpr auto set_double_sided(const bool val) -> void
  {
    if (val)
      flags |= FLAG_DOUBLE_SIDED;
    else
      flags &= ~FLAG_DOUBLE_SIDED;
  }

  constexpr auto set_emissive(const bool val) -> void
  {
    if (val)
      flags |= FLAG_EMISSIVE;
    else
      flags &= ~FLAG_EMISSIVE;
  }

  constexpr auto set_has_albedo_texture(const bool val) -> void
  {
    if (val)
      flags |= FLAG_ALBEDO_TEXTURE;
    else
      flags &= ~FLAG_ALBEDO_TEXTURE;
  }

  constexpr auto set_has_normal_map(const bool val) -> void
  {
    if (val)
      flags |= FLAG_NORMAL_MAP;
    else
      flags &= ~FLAG_NORMAL_MAP;
  }

  constexpr auto set_has_roughness_map(const bool val) -> void
  {
    if (val)
      flags |= FLAG_ROUGHNESS_MAP;
    else
      flags &= ~FLAG_ROUGHNESS_MAP;
  }

  constexpr auto set_has_metallic_map(const bool val) -> void
  {
    if (val)
      flags |= FLAG_METALLIC_MAP;
    else
      flags &= ~FLAG_METALLIC_MAP;
  }

  constexpr auto set_has_ao_map(const bool val) -> void
  {
    if (val)
      flags |= FLAG_AO_MAP;
    else
      flags &= ~FLAG_AO_MAP;
  }

  constexpr auto set_has_emissive_map(const bool val) -> void
  {
    if (val)
      flags |= FLAG_EMISSIVE_MAP;
    else
      flags &= ~FLAG_EMISSIVE_MAP;
  }
};

static_assert(sizeof(MaterialData) <= 128,
              "MaterialData exceeds recommended push constant size");
