#pragma once

#include "core/forward.hpp"
#include "dynamic_rendering/assets/pointer.hpp"

#include <glm/glm.hpp>
#include <vector>

struct BloomMip
{
  Assets::Pointer<Image> image;
  Assets::Pointer<Material> blur_horizontal;
  Assets::Pointer<Material> blur_vertical;
  Assets::Pointer<Material> downsample_material;
  Assets::Pointer<Material> upsample_material;
};

struct BloomPass
{
  explicit BloomPass(const Device& device, const Image*, int mip_count = 5);
  ~BloomPass() = default;

  auto prepare(std::uint32_t) -> void;
  void resize(const glm::uvec2& size);
  void resize(std::uint32_t w, std::uint32_t h)
  {
    return resize(glm::uvec2{ w, h });
  }
  void update_source(const Image* image);
  void record(VkCommandBuffer cmd, DescriptorSetManager&, uint32_t frame_index);

  [[nodiscard]] auto get_output_image() const -> const Image&;

private:
  const Device* device;
  const Image* source_image = nullptr;

  Assets::Pointer<Image> extract_image;
  Assets::Pointer<Material> extract_material;
  Assets::Pointer<Image> blur_temp;

  std::vector<BloomMip> mip_chain;

  void dispatch_compute(VkCommandBuffer cmd,
                        const Material& material,
                        std::span<const VkDescriptorSet>,
                        glm::uvec2 extent);

  void downsample_and_blur(VkCommandBuffer cmd,
                           const DescriptorSetManager& dsm,
                           uint32_t frame_index);
  void downsample(VkCommandBuffer, DescriptorSetManager&, uint32_t);
  void blur_mips(VkCommandBuffer, DescriptorSetManager&, uint32_t);
  void upsample_and_combine(VkCommandBuffer,
                            const DescriptorSetManager&,
                            uint32_t);
};
