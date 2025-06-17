#pragma once

#include "assets/asset_allocator.hpp"
#include "core/forward.hpp"
#include "core/util.hpp"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

/// Binding is shader size (uniform <binding>) and source is the renderer's
/// name.
struct FullscreenInputBinding
{
  std::string binding;
  std::string source;
};

struct FullscreenOutputBinding
{
  std::string binding;
  std::string name;
  std::string extent;
  VkFormat format;
  VkImageUsageFlags usage;
};

struct FullscreenTechniqueMetadata
{
  std::string path;
  std::string type;

  [[nodiscard]] auto as_path() const -> std::filesystem::path { return path; }
};

struct FullscreenTechniqueDescription
{
  std::string name;
  std::string material;
  FullscreenTechniqueMetadata metadata;
  std::vector<FullscreenInputBinding> inputs;
  FullscreenOutputBinding output;
  VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS;
};

struct IFullscreenTechnique
{
  virtual ~IFullscreenTechnique() = default;
  virtual auto perform(const CommandBuffer&, std::uint32_t) const -> void = 0;
  virtual auto initialise(Renderer&,
                          const string_hash_map<const Image*>&,
                          const string_hash_map<const GPUBuffer*>&) -> void = 0;
  [[nodiscard]] virtual auto get_material() const -> Material* = 0;
};

class FullscreenTechniqueBase : public IFullscreenTechnique
{
public:
  FullscreenTechniqueBase(const Device& dev,
                          DescriptorSetManager& dsm,
                          FullscreenTechniqueDescription desc)
    : device(&dev)
    , descriptors(&dsm)
    , desc(std::move(desc))
  {
  }
  auto initialise(Renderer&,
                  const string_hash_map<const Image*>&,
                  const string_hash_map<const GPUBuffer*>&) -> void override
  {
  }

protected:
  const Device* device;
  DescriptorSetManager* descriptors;
  FullscreenTechniqueDescription desc;
};

class FullscreenTechniqueFactory
{
public:
  static auto create(std::string_view, const Device&, DescriptorSetManager&)
    -> Assets::Pointer<IFullscreenTechnique>;
};
