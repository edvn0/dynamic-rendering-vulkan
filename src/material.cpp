#include "material.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "image.hpp"

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

auto
reflect_shader_using_spirv_reflect(
  std::string_view file_path,
  std::vector<VkDescriptorSetLayoutBinding>& bindings,
  std::vector<VkPushConstantRange>& push_constant_ranges) -> void
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
  -> std::unique_ptr<Material>
{
  if (!Material::pipeline_factory || !Material::compute_pipeline_factory) {
    Material::pipeline_factory = std::make_unique<PipelineFactory>(device);
    Material::compute_pipeline_factory =
      std::make_unique<ComputePipelineFactory>(device);
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkPushConstantRange> push_constants;

  for (auto const& stage : blueprint.shader_stages) {
    if (stage.empty)
      continue;
    reflect_shader_using_spirv_reflect(
      stage.filepath, bindings, push_constants);
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

#ifdef ACCEPT_DEBUG
  if (bindings.empty() && is_compute) {
    VkDescriptorSetLayoutBinding ssbo_binding{
      .binding = 0,
      .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
      .descriptorCount = 1,
      .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
      .pImmutableSamplers = nullptr,
    };
    bindings.push_back(ssbo_binding);
  }
#endif

  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .bindingCount = static_cast<uint32_t>(bindings.size()),
    .pBindings = bindings.data(),
  };

  VkDescriptorSetLayout material_set_layout{ VK_NULL_HANDLE };
  vkCreateDescriptorSetLayout(
    device.get_device(), &layout_info, nullptr, &material_set_layout);

  frame_array<VkDescriptorSet> descriptor_sets;
  descriptor_sets.fill(VK_NULL_HANDLE);

  std::unordered_map<VkDescriptorType, uint32_t> type_counts;
  for (auto const& b : bindings) {
    type_counts[b.descriptorType] += b.descriptorCount;
  }

  std::vector<VkDescriptorPoolSize> pool_sizes;
  pool_sizes.reserve(type_counts.size());
  for (auto const& [type, count_per_set] : type_counts) {
    pool_sizes.push_back(VkDescriptorPoolSize{
      .type = type, .descriptorCount = count_per_set * image_count });
  }

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
    .pNext = nullptr,
    .flags = 0,
    .maxSets = image_count,
    .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
    .pPoolSizes = pool_sizes.data(),
  };

  VkDescriptorPool descriptor_pool{ VK_NULL_HANDLE };
  vkCreateDescriptorPool(
    device.get_device(), &pool_info, nullptr, &descriptor_pool);

  std::array<VkDescriptorSetLayout, image_count> layouts;
  layouts.fill(material_set_layout);

  VkDescriptorSetAllocateInfo alloc_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
    .pNext = nullptr,
    .descriptorPool = descriptor_pool,
    .descriptorSetCount = image_count,
    .pSetLayouts = layouts.data(),
  };

  vkAllocateDescriptorSets(
    device.get_device(), &alloc_info, descriptor_sets.data());

  std::vector<VkDescriptorSetLayout> descriptor_set_layouts{
    material_set_layout
  };
  const auto* chosen_factory =
    is_compute ? compute_pipeline_factory.get() : pipeline_factory.get();

  auto pipeline = chosen_factory->create_pipeline(
    blueprint,
    {
      .renderer_set_layout = renderer_set_layout,
      .material_sets = descriptor_set_layouts,
      .push_constants = push_constants,
    });

  assert(pipeline != nullptr && "Pipeline creation failed.");
  return std::unique_ptr<Material>(
    new Material(device,
                 std::move(descriptor_sets),
                 std::span(descriptor_set_layouts),
                 descriptor_pool,
                 std::move(pipeline)));
}

Material::Material(const Device& dev,
                   frame_array<VkDescriptorSet>&& sets,
                   std::span<const VkDescriptorSetLayout> ls,
                   VkDescriptorPool p,
                   std::unique_ptr<CompiledPipeline> pipe)
  : device(&dev)
  , descriptor_sets(std::move(sets))
  , descriptor_set_layout(ls[0])
  , descriptor_pool(p)
  , pipeline(std::move(pipe))
{
}

auto
Material::upload(const std::string_view name, const GPUBuffer* buffer) -> void
{
  auto key = std::string(name);
  if (buffers[key] != buffer) {
    buffers[key] = buffer;
    for (auto& dirty : per_frame_dirty_flags) {
      dirty.insert(key);
    }
  }
}

auto
Material::upload(const std::string_view name, const Image* image) -> void
{
  auto key = std::string(name);
  if (images[key] != image) {
    images[key] = image;
    for (auto& dirty : per_frame_dirty_flags) {
      dirty.insert(key);
    }
  }
}

auto
Material::prepare_for_rendering(std::uint32_t frame_index)
  -> const VkDescriptorSet&
{
  auto& dirty_bindings = per_frame_dirty_flags[frame_index];
  if (dirty_bindings.empty())
    return descriptor_sets[frame_index];

  std::vector<VkWriteDescriptorSet> writes;
  std::vector<VkDescriptorBufferInfo> buffer_infos;

  for (const auto& name : dirty_bindings) {
    const auto it = buffers.find(name);
    if (it != buffers.end()) {
      const GPUBuffer* buffer = it->second;

      VkDescriptorBufferInfo buffer_info{
        .buffer = buffer->get(),
        .offset = 0,
        .range = VK_WHOLE_SIZE,
      };
      buffer_infos.push_back(buffer_info);

      std::uint32_t binding = 0;
      VkWriteDescriptorSet write{
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .pNext = nullptr,
        .dstSet = descriptor_sets[frame_index],
        .dstBinding = binding,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pImageInfo = nullptr,
        .pBufferInfo = &buffer_infos.back(),
        .pTexelBufferView = nullptr,
      };
      writes.push_back(write);
    }
  }

  vkUpdateDescriptorSets(device->get_device(),
                         static_cast<uint32_t>(writes.size()),
                         writes.data(),
                         0,
                         nullptr);

  dirty_bindings.clear();
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
  buffers.clear();
  images.clear();
}