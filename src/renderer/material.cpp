#include "renderer/material.hpp"

#include "renderer/material_bindings.hpp"

#include "core/device.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>

#include <spirv_reflect.h>

static constexpr auto equal = [](const VkDescriptorSetLayoutBinding& a,
                                 const VkDescriptorSetLayoutBinding& b) {
  return a.binding == b.binding && a.descriptorType == b.descriptorType &&
         a.descriptorCount == b.descriptorCount;
};

static auto
merge_descriptor_binding(std::vector<VkDescriptorSetLayoutBinding>& dst,
                         VkDescriptorSetLayoutBinding const& binding) -> void
{
  for (auto& ex : dst) {
    if (equal(ex, binding)) {
      ex.stageFlags |= binding.stageFlags;
      return;
    }
  }
  dst.push_back(binding);
}

static auto
merge_push_constant_range(std::vector<VkPushConstantRange>& dst,
                          VkPushConstantRange const& range) -> void
{
  for (auto& ex : dst) {
    if (ex.offset == range.offset && ex.size == range.size) {
      ex.stageFlags |= range.stageFlags;
      return;
    }
  }
  dst.push_back(range);
}

static auto
handle_block_declaration_naming(const SpvReflectDescriptorBinding* refl,
                                auto& binding_info,
                                const SpvReflectDescriptorSet* ds)
{
  if (std::strlen(refl->name) > 0) {

    binding_info[refl->name] = std::make_tuple(ds->set, refl->binding);
  } else {
    const auto* type_name = refl->type_description->type_name;
    binding_info[type_name] = std::make_tuple(ds->set, refl->binding);
  }
}

auto
reflect_shader_using_spirv_reflect(
  std::string_view file_path,
  std::vector<VkDescriptorSetLayoutBinding>& bindings,
  std::vector<VkPushConstantRange>& push_constant_ranges,
  std::unordered_map<std::string, std::tuple<std::uint32_t, std::uint32_t>>&
    binding_info) -> void
{
  std::vector<std::uint8_t> shader_code;
  if (!Shader::load_binary(file_path, shader_code)) {
    std::cerr << "Failed to load shader: " << file_path << "\n";
    return;
  }
  spv_reflect::ShaderModule shader_module(shader_code.size(),
                                          shader_code.data());
  if (shader_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
    std::cerr << "Failed to reflect shader: " << file_path << "\n";
    return;
  }

  VkShaderStageFlags vk_stage = 0;
  switch (shader_module.GetShaderStage()) {
    case SPV_REFLECT_SHADER_STAGE_VERTEX_BIT:
      vk_stage = VK_SHADER_STAGE_VERTEX_BIT;
      break;
    case SPV_REFLECT_SHADER_STAGE_FRAGMENT_BIT:
      vk_stage = VK_SHADER_STAGE_FRAGMENT_BIT;
      break;
    case SPV_REFLECT_SHADER_STAGE_COMPUTE_BIT:
      vk_stage = VK_SHADER_STAGE_COMPUTE_BIT;
      break;
    default:
      std::cerr << "Unknown SPIR-V stage: " << file_path << "\n";
      return;
  }

  // --- descriptors ---
  uint32_t set_count = 0;
  shader_module.EnumerateDescriptorSets(&set_count, nullptr);
  std::vector<SpvReflectDescriptorSet*> sets(set_count);
  shader_module.EnumerateDescriptorSets(&set_count, sets.data());

  for (const auto* ds : sets) {
    if (ds->set != 1)
      continue;
    for (uint32_t i = 0; i < ds->binding_count; ++i) {
      const auto* refl = ds->bindings[i];
      VkDescriptorSetLayoutBinding b{};
      b.binding = refl->binding;
      b.descriptorType = VkDescriptorType(refl->descriptor_type);
      b.descriptorCount = refl->count;
      b.stageFlags = vk_stage;
      b.pImmutableSamplers = nullptr;

      merge_descriptor_binding(bindings, b);

      /** This business logic handles
      ** layout(set = 1, binding = 0) uniform SomeUBO { some stuff... } ubo;
      ** layout(set = 1, binding = 1) uniform SomeOtherUBO { some stuff... };
      ** we prio the declaration name "ubo" over the block name "SomeUBO".
      */
      handle_block_declaration_naming(refl, binding_info, ds);
    }
  }

  // --- push constants ---
  uint32_t pc_count = 0;
  shader_module.EnumeratePushConstantBlocks(&pc_count, nullptr);
  std::vector<SpvReflectBlockVariable*> pcs(pc_count);
  shader_module.EnumeratePushConstantBlocks(&pc_count, pcs.data());

  for (const auto* pc : pcs) {
    VkPushConstantRange r{};
    r.offset = pc->offset;
    r.size = pc->size;
    r.stageFlags = vk_stage;

    merge_push_constant_range(push_constant_ranges, r);
  }
}

auto
Material::create(const Device& device,
                 const PipelineBlueprint& blueprint,
                 VkDescriptorSetLayout renderer_set_layout)
  -> std::expected<std::unique_ptr<Material>, MaterialError>
{
  if (!pipeline_factory || !compute_pipeline_factory) {
    pipeline_factory = std::make_unique<PipelineFactory>(device);
    compute_pipeline_factory = std::make_unique<ComputePipelineFactory>(device);
  }

  std::vector<VkDescriptorSetLayoutBinding> binds;
  std::vector<VkPushConstantRange> push_constants;
  std::unordered_map<std::string, std::tuple<std::uint32_t, std::uint32_t>>
    binds_info;

  for (const auto& stage : blueprint.shader_stages) {
    if (!stage.empty)
      reflect_shader_using_spirv_reflect(
        stage.filepath, binds, push_constants, binds_info);
  }

  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<std::uint32_t>(binds.size()),
    .pBindings = binds.data(),
  };

  VkDescriptorSetLayout material_set_layout{ VK_NULL_HANDLE };
  if (vkCreateDescriptorSetLayout(
        device.get_device(), &layout_info, nullptr, &material_set_layout) !=
      VK_SUCCESS)
    return std::unexpected(MaterialError{
      .message = "Failed to create descriptor set layout",
      .code = MaterialError::Code::descriptor_layout_failed,
    });

  frame_array<VkDescriptorSet> sets{};
  sets.fill(VK_NULL_HANDLE);

  std::unordered_map<VkDescriptorType, std::uint32_t> type_counts;
  for (const auto& b : binds)
    type_counts[b.descriptorType] += b.descriptorCount;

  std::vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(type_counts.size());
  for (const auto& [type, count] : type_counts)
    pool_sizes.push_back({
      .type = type,
      .descriptorCount = count * image_count,
    });

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .maxSets = image_count,
    .poolSizeCount = static_cast<std::uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  VkDescriptorPool pool{ VK_NULL_HANDLE };
  if (vkCreateDescriptorPool(device.get_device(), &pool_info, nullptr, &pool) !=
      VK_SUCCESS)
    return std::unexpected(MaterialError{
      .message = "Failed to create descriptor pool",
      .code = MaterialError::Code::pool_creation_failed,
    });

  std::array<VkDescriptorSetLayout, image_count> layouts{};
  layouts.fill(material_set_layout);

  VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .descriptorPool = pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };

  if (vkAllocateDescriptorSets(device.get_device(), &alloc_info, sets.data()) !=
      VK_SUCCESS)
    return std::unexpected(MaterialError{
      .message = "Failed to allocate descriptor sets",
      .code = MaterialError::Code::descriptor_allocation_failed });

  auto factory = blueprint.shader_stages.size() == 1 &&
                     blueprint.shader_stages[0].stage == ShaderStage::compute
                   ? compute_pipeline_factory.get()
                   : pipeline_factory.get();

  auto pipeline_result =
    factory->create_pipeline(blueprint,
                             PipelineLayoutInfo{
                               .renderer_set_layout = renderer_set_layout,
                               .material_sets = { material_set_layout },
                               .push_constants = push_constants,
                             });

  if (!pipeline_result)
    return std::unexpected(MaterialError{
      .message = "Pipeline creation failed: " + pipeline_result.error().message,
      .code = MaterialError::Code::pipeline_error,
      .inner_pipeline_error = std::move(pipeline_result.error()) });

  std::unique_ptr<Material> mat;
  mat.reset(new Material(device,
                         std::move(sets),
                         std::span{ &material_set_layout, 1 },
                         pool,
                         std::move(*pipeline_result),
                         std::move(binds_info)));
  mat->pipeline_hash = blueprint.hash();
  return mat;
}

auto
Material::invalidate(const Image* image) -> void
{
  for (const auto& [key, binding] : bindings) {
    const auto* img_binding = dynamic_cast<ImageBinding*>(binding.get());
    if (img_binding && img_binding->get_underlying_image() == image) {
      for (auto& dirty_set : per_frame_dirty_flags) {
        dirty_set.insert(key);
      }
    }
  }
}

auto
Material::reload(const PipelineBlueprint& blueprint,
                 VkDescriptorSetLayout renderer_set_layout) -> void
{
  const auto new_hash = blueprint.hash();
  if (new_hash != 0 && new_hash == pipeline_hash) {
    std::cout << "Pipeline already up to date\n";
    return;
  }

  auto rebuilt_result = rebuild_pipeline(blueprint, renderer_set_layout);
  if (!rebuilt_result) {
    std::cerr << "Failed to rebuild pipeline: "
              << rebuilt_result.error().message << "\n";
    return;
  }

  std::cout << "Rebuilt pipeline for material\n";
  pipeline = std::move(*rebuilt_result);
  pipeline_hash = new_hash;
}

Material::Material(
  const Device& dev,
  frame_array<VkDescriptorSet>&& sets,
  std::span<const VkDescriptorSetLayout> ls,
  VkDescriptorPool p,
  std::unique_ptr<CompiledPipeline> pipe,
  std::unordered_map<std::string, std::tuple<std::uint32_t, std::uint32_t>>&&
    binds)
  : binding_info(std::move(binds))
  , device(&dev)
  , descriptor_sets(std::move(sets))
  , descriptor_set_layout(ls[0])
  , descriptor_pool(p)
  , pipeline(std::move(pipe))
  , pipeline_hash()
{
}

auto
Material::rebuild_pipeline(const PipelineBlueprint& blueprint,
                           VkDescriptorSetLayout renderer_set_layout)
  -> std::expected<std::unique_ptr<CompiledPipeline>, MaterialError>
{
  std::vector<VkPushConstantRange> push_constants;
  std::vector<VkDescriptorSetLayoutBinding> new_bindings;

  binding_info.clear();

  for (const auto& stage : blueprint.shader_stages) {
    if (!stage.empty)
      reflect_shader_using_spirv_reflect(
        stage.filepath, new_bindings, push_constants, binding_info);
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

  const auto* factory =
    is_compute ? compute_pipeline_factory.get() : pipeline_factory.get();

  auto result =
    factory->create_pipeline(blueprint,
                             PipelineLayoutInfo{
                               .renderer_set_layout = renderer_set_layout,
                               .material_sets = { descriptor_set_layout },
                               .push_constants = push_constants,
                             });

  if (!result)
    return std::unexpected(MaterialError{
      .message = "Failed to rebuild pipeline: " + result.error().message,
      .code = MaterialError::Code::pipeline_error,
      .inner_pipeline_error = result.error() });

  return std::move(*result);
}

auto
Material::upload(std::string_view name, const GPUBuffer* buffer) -> void
{
  auto key = std::string(name);
  auto it = bindings.find(key);

  const bool is_uniform =
    (buffer->get_usage_flags() & VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  auto descriptor_type = is_uniform ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER
                                    : VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

  const auto binding_it = binding_info.find(key);
  if (binding_it == binding_info.end())
    return;

  const uint32_t binding_index = std::get<1>(binding_it->second);

  bool needs_update = true;
  if (it != bindings.end()) {
    const auto* existing = dynamic_cast<BufferBinding*>(it->second.get());
    if (existing && existing->get_underlying_buffer() == buffer) {
      needs_update = false;
    }
  }

  if (needs_update) {
    bindings[key] =
      std::make_unique<BufferBinding>(buffer, descriptor_type, binding_index);
    for (auto& dirty : per_frame_dirty_flags) {
      dirty.insert(key);
    }
  }
}

auto
Material::upload(std::string_view name, const Image* image) -> void
{
  auto key = std::string(name);
  auto it = bindings.find(key);

  const auto binding_it = binding_info.find(key);
  if (binding_it == binding_info.end())
    return;

  const uint32_t binding_index = std::get<1>(binding_it->second);

  bool needs_update = true;
  if (it != bindings.end()) {
    const auto* existing = dynamic_cast<ImageBinding*>(it->second.get());
    if (existing && existing->get_underlying_image() == image) {
      needs_update = false;
    }
  }

  if (needs_update) {
    bindings[key] = std::make_unique<ImageBinding>(
      image, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, binding_index);
    for (auto& dirty : per_frame_dirty_flags) {
      dirty.insert(key);
    }
  }
}

auto
Material::prepare_for_rendering(std::uint32_t frame_index)
  -> const VkDescriptorSet&
{
  auto& dirty = per_frame_dirty_flags[frame_index];
  if (dirty.empty())
    return descriptor_sets[frame_index];

  std::vector<VkWriteDescriptorSet> writes;
  writes.reserve(dirty.size());

  for (const auto& name : dirty) {
    auto it = bindings.find(name);
    if (it == bindings.end())
      continue;
    writes.push_back(
      it->second->write_descriptor(frame_index, descriptor_sets[frame_index]));
  }

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<uint32_t>(writes.size()),
                         writes.data(),
                         0,
                         nullptr);

  dirty.clear();
  return descriptor_sets[frame_index];
}
Material::~Material()
{
  destroy();
}

auto
Material::destroy() -> void
{
  if (destroyed)
    return;
  destroyed = true;

  vkDestroyDescriptorPool(device->get_device(), descriptor_pool, nullptr);
  vkDestroyDescriptorSetLayout(
    device->get_device(), descriptor_set_layout, nullptr);
  descriptor_sets.fill(VK_NULL_HANDLE);

  pipeline.reset();
}