#pragma once

#include "assets/pointer.hpp"
#include "renderer/environments.hpp"

#include "renderer/techniques/fullscreen_technique.hpp"

#include "renderer/material.hpp"

class ShadowGUITechnique final : public FullscreenTechniqueBase
{
public:
  ShadowGUITechnique(const Device& dev,
                     const std::string_view name,
                     DescriptorSetManager& dsm,
                     FullscreenTechniqueDescription desc)
    : FullscreenTechniqueBase(dev, dsm, std::move(desc))
  {
    if (auto mat = Material::create(*device, name); mat.has_value()) {
      material = std::move(mat.value());
    }
  }
  ~ShadowGUITechnique() override = default;

  auto initialise(Renderer&,
                  const string_hash_map<const Image*>&,
                  const string_hash_map<const GPUBuffer*>&) -> void override;
  auto on_resize(std::uint32_t, std::uint32_t) -> void override;
  auto perform(const CommandBuffer&, std::uint32_t) const -> void override;
  [[nodiscard]] auto get_material() const -> Material* override
  {
    return material.get();
  }
  [[nodiscard]] auto valid() const { return material != nullptr; }

  [[nodiscard]] auto get_output() const -> const Image* override
  {
    return output_image.get();
  }

private:
  Assets::Pointer<Material> material;
  Assets::Pointer<Image> output_image;
};