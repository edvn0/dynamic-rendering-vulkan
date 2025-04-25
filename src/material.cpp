#include "material.hpp"
#include "device.hpp"
#include "gpu_buffer.hpp"
#include "image.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>

#include <spirv_reflect.h>

auto
reflect_shader_using_spirv_reflect(
  const auto& file_path,
  std::vector<VkDescriptorSetLayoutBinding>& bindings) -> void
{
  auto shader_code = Shader::load_binary(file_path);
  spv_reflect::ShaderModule reflect_module(shader_code.size(),
                                           shader_code.data());
  if (reflect_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {

    auto error_code = reflect_module.GetResult();
    std::cerr << "Failed to reflect shader: " << file_path
              << ", error code: " << error_code << std::endl;
    return;
  }

  uint32_t set_count = 0;
  SpvReflectDescriptorSet* sets = nullptr;
  reflect_module.EnumerateDescriptorSets(&set_count, &sets);

  if (set_count == 0) {
    std::cerr << "No descriptor sets found in shader." << std::endl;
    return;
  }

  // We only want the set = 1 bindings.
  for (uint32_t i = 0; i < set_count; ++i) {
    if (sets[i].set == 1) {
      for (uint32_t j = 0; j < sets[i].binding_count; ++j) {
        VkDescriptorSetLayoutBinding binding;
        binding.binding = sets[i].bindings[j]->binding;
        binding.descriptorType =
          static_cast<VkDescriptorType>(sets[i].bindings[j]->descriptor_type);
        binding.descriptorCount = sets[i].bindings[j]->count;
        // TODO: binding.stageFlags = sets[i].bindings[j].;
        binding.pImmutableSamplers = nullptr;
        bindings.push_back(binding);
      }
    }
  }
}

auto
Material::create(const Device& device,
                 const PipelineBlueprint& blueprint,
                 VkDescriptorSetLayout renderer_set_layout)
  -> std::unique_ptr<Material>
{
  if (!pipeline_factory || !compute_pipeline_factory) {
    pipeline_factory = std::make_unique<PipelineFactory>(device);
    compute_pipeline_factory = std::make_unique<ComputePipelineFactory>(device);
  }

  std::vector<VkDescriptorSetLayoutBinding> bindings;
  for (const auto& stage : blueprint.shader_stages) {
    reflect_shader_using_spirv_reflect(stage.filepath, bindings);
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

  VkDescriptorSetLayoutBinding ssbo_binding{
    .binding = 0,
    .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
    .descriptorCount = 1,
    .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
    .pImmutableSamplers = nullptr,
  };

  std::array binds{ ssbo_binding };

  VkDescriptorSetLayoutCreateInfo layout_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    .bindingCount = static_cast<uint32_t>(binds.size()),
    .pBindings = binds.data(),
  };

  VkDescriptorSetLayout material_set_layout{ VK_NULL_HANDLE };
  vkCreateDescriptorSetLayout(
    device.get_device(), &layout_info, nullptr, &material_set_layout);

  frame_array<VkDescriptorSet> descriptor_sets;
  descriptor_sets.fill(VK_NULL_HANDLE);

  std::array<VkDescriptorPoolSize, 1> pool_sizes{
    VkDescriptorPoolSize{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, image_count },
  };

  VkDescriptorPoolCreateInfo pool_info{
    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
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
    });

  assert(pipeline != nullptr && "Pipeline creation failed.");
  return std::unique_ptr<Material>(
    new Material(device,
                 std::move(descriptor_sets),
                 std::span(descriptor_set_layouts),
                 descriptor_pool,
                 std::move(pipeline),
                 is_compute));
}

Material::Material(const Device& dev,
                   frame_array<VkDescriptorSet>&& sets,
                   std::span<const VkDescriptorSetLayout> ls,
                   VkDescriptorPool p,
                   std::unique_ptr<CompiledPipeline> pipe,
                   bool is_compute)
  : device(&dev)
  , descriptor_sets(std::move(sets))
  , descriptor_set_layout(ls[0])
  , descriptor_pool(p)
  , pipeline(std::move(pipe))
  , is_compute(is_compute)
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
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        .pBufferInfo = &buffer_infos.back(),
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