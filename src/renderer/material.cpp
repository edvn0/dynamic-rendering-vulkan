#include "renderer/material.hpp"

#include "renderer/material_bindings.hpp"

#include "core/device.hpp"
#include "core/gpu_buffer.hpp"
#include "core/image.hpp"
#include "renderer/renderer.hpp"

#include <algorithm>
#include <cassert>
#include <unordered_set>

#include <spirv_reflect.h>

static constexpr auto equal = [](const VkDescriptorSetLayoutBinding& a,
                                 const VkDescriptorSetLayoutBinding& b) {
  return a.binding == b.binding && a.descriptorType == b.descriptorType &&
         a.descriptorCount == b.descriptorCount;
};

struct descriptor_binding_hash
{
  auto operator()(const VkDescriptorSetLayoutBinding& b) const noexcept
    -> std::size_t
  {
    std::size_t h = 0;
    h ^=
      std::hash<std::uint32_t>{}(b.binding) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<std::uint32_t>{}(b.descriptorType) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    h ^= std::hash<std::uint32_t>{}(b.descriptorCount) + 0x9e3779b9 + (h << 6) +
         (h >> 2);
    return h;
  }
};

struct descriptor_binding_equal
{
  auto operator()(const VkDescriptorSetLayoutBinding& a,
                  const VkDescriptorSetLayoutBinding& b) const noexcept -> bool
  {
    return equal(a, b);
  }
};

using BindingSet = std::unordered_set<VkDescriptorSetLayoutBinding,
                                      descriptor_binding_hash,
                                      descriptor_binding_equal>;

static auto
merge_descriptor_binding(BindingSet& dst, VkDescriptorSetLayoutBinding& binding)
  -> void
{
  if (binding.descriptorType == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
    static constexpr auto stages = VK_SHADER_STAGE_VERTEX_BIT |
                                   VK_SHADER_STAGE_FRAGMENT_BIT |
                                   VK_SHADER_STAGE_COMPUTE_BIT;
    binding.stageFlags = stages;
  }
  if (binding.descriptorType == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER ||
      binding.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
    binding.stageFlags =
      VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_COMPUTE_BIT;
  }

  dst.insert(binding);
}

static auto
merge_push_constant_range(std::vector<VkPushConstantRange>& dst,
                          VkPushConstantRange const& range) -> void
{
  for (auto& [stageFlags, offset, size] : dst) {
    if (offset == range.offset && size == range.size) {
      stageFlags |= range.stageFlags;
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
  const std::string_view file_path,
  std::unordered_map<std::uint32_t, BindingSet>& bindings,
  std::vector<VkPushConstantRange>& push_constant_ranges,
  string_hash_map<std::tuple<std::uint32_t, std::uint32_t>>& binding_info)
  -> void
{
  std::vector<std::uint8_t> shader_code;
  if (!Shader::load_binary(file_path, shader_code)) {
    Logger::log_error("Failed to load shader: {}", file_path);
    return;
  }
  const spv_reflect::ShaderModule shader_module(shader_code.size(),
                                                shader_code.data());
  if (shader_module.GetResult() != SPV_REFLECT_RESULT_SUCCESS) {
    Logger::log_error("Failed to reflect shader: {}", file_path);
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
      Logger::log_error("Unsupported shader stage in shader: {}", file_path);
      return;
  }

  // --- descriptors ---
  uint32_t set_count = 0;
  shader_module.EnumerateDescriptorSets(&set_count, nullptr);
  std::vector<SpvReflectDescriptorSet*> sets;
  if (set_count > 0) {
    sets.resize(set_count);
    shader_module.EnumerateDescriptorSets(&set_count, sets.data());
  }

  for (const auto* ds : sets) {
    for (uint32_t i = 0; i < ds->binding_count; ++i) {
      const auto* refl = ds->bindings[i];
      VkDescriptorSetLayoutBinding b{};
      b.binding = refl->binding;
      b.descriptorType = static_cast<VkDescriptorType>(refl->descriptor_type);
      b.descriptorCount = refl->count;
      b.stageFlags = vk_stage;
      b.pImmutableSamplers = nullptr;

      merge_descriptor_binding(bindings[ds->set], b);

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
  std::vector<SpvReflectBlockVariable*> pcs;
  if (pc_count > 0) {
    pcs.resize(pc_count);
    shader_module.EnumeratePushConstantBlocks(&pc_count, pcs.data());
  }

  for (const auto* pc : pcs) {
    VkPushConstantRange r{};
    r.offset = pc->offset;
    r.size = pc->size;
    r.stageFlags = vk_stage;

    merge_push_constant_range(push_constant_ranges, r);
  }
}

auto
Material::create(const Device& device, const PipelineBlueprint& blueprint)
  -> std::expected<std::unique_ptr<Material>, MaterialError>
{
  if (!pipeline_factory || !compute_pipeline_factory) {
    pipeline_factory = std::make_unique<PipelineFactory>(device);
    compute_pipeline_factory = std::make_unique<ComputePipelineFactory>(device);
  }

  std::unordered_map<std::uint32_t, BindingSet> binds;
  std::vector<VkPushConstantRange> push_constants;
  string_hash_map<std::tuple<std::uint32_t, std::uint32_t>> binds_info;

  for (const auto& stage : blueprint.shader_stages) {
    if (!stage.empty)
      reflect_shader_using_spirv_reflect(
        stage.filepath, binds, push_constants, binds_info);
  }

  std::vector<VkDescriptorSetLayout> all_layouts(binds.size(), VK_NULL_HANDLE);
  for (auto& [set, binding_set] : binds) {
    std::vector<VkDescriptorSetLayoutBinding> binding_vec{ binding_set.begin(),
                                                           binding_set.end() };

    VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(binding_vec.size()),
      .pBindings = binding_vec.data(),
    };

    if (vkCreateDescriptorSetLayout(
          device.get_device(), &layout_info, nullptr, &all_layouts.at(set)) !=
        VK_SUCCESS)
      return std::unexpected(MaterialError{
        .message = "Failed to create descriptor set layout",
        .code = MaterialError::Code::descriptor_layout_failed,
      });
  }

  frame_array<VkDescriptorSet> material_descriptor_sets{};
  material_descriptor_sets.fill(VK_NULL_HANDLE);
  VkDescriptorPool pool{ VK_NULL_HANDLE };

  std::vector<VkDescriptorSetLayout> material_sets;
  if (binds.contains(0)) {
    material_sets.push_back(all_layouts.at(0));
  }

  if (binds.contains(1)) {
    std::unordered_map<VkDescriptorType, uint32_t> type_counts;
    for (const auto& b : binds.at(1))
      type_counts[b.descriptorType] += b.descriptorCount;

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (const auto& [type, count] : type_counts)
      pool_sizes.push_back(
        { .type = type, .descriptorCount = count * frames_in_flight });

    VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = frames_in_flight,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
    };

    if (vkCreateDescriptorPool(
          device.get_device(), &pool_info, nullptr, &pool) != VK_SUCCESS)
      return std::unexpected(MaterialError{
        .message = "Failed to create descriptor pool",
        .code = MaterialError::Code::pool_creation_failed,
      });

    std::vector repeated_layouts(frames_in_flight, all_layouts.at(1));
    VkDescriptorSetAllocateInfo alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = pool,
      .descriptorSetCount = frames_in_flight,
      .pSetLayouts = repeated_layouts.data(),
    };

    if (vkAllocateDescriptorSets(device.get_device(),
                                 &alloc_info,
                                 material_descriptor_sets.data()) != VK_SUCCESS)
      return std::unexpected(MaterialError{
        .message = "Failed to allocate descriptor sets",
        .code = MaterialError::Code::descriptor_allocation_failed,
      });

    material_sets.push_back(all_layouts.at(1));
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

  const auto* factory =
    is_compute ? compute_pipeline_factory.get() : pipeline_factory.get();

  auto pipeline_result =
    factory->create_pipeline(blueprint,
                             PipelineLayoutInfo{
                               .material_sets = material_sets,
                               .push_constants = push_constants,
                             });

  if (!pipeline_result)
    return std::unexpected(MaterialError{
      .message = "Pipeline creation failed: " + pipeline_result.error().message,
      .code = MaterialError::Code::pipeline_error,
      .inner_pipeline_error = std::move(pipeline_result.error()),
    });

  // Store all necessary descriptor set layouts
  std::vector<VkDescriptorSetLayout> layouts_to_store;
  for (const auto& layout : all_layouts) {
    if (layout != VK_NULL_HANDLE) {
      layouts_to_store.push_back(layout);
    }
  }

  std::unique_ptr<Material> mat;
  mat.reset(new Material(device,
                         std::move(material_descriptor_sets),
                         std::move(layouts_to_store),
                         pool,
                         std::move(*pipeline_result),
                         std::move(binds_info)));
  mat->pipeline_hash = blueprint.hash();
  return mat;
}

auto
Material::create(const Device& device, const std::string_view name)
  -> std::expected<Assets::Pointer<Material>, MaterialError>
{
  return Material::create(device, device.get_blueprint(name));
}

auto
Material::initialise(const Device& d, const std::string_view blueprint) -> bool
{
  return initialise(d, d.get_blueprint(blueprint));
}

auto
Material::initialise(const Device& d, const PipelineBlueprint& blueprint)
  -> bool
{
  device = &d;
  if (!pipeline_factory || !compute_pipeline_factory) {
    pipeline_factory = std::make_unique<PipelineFactory>(d);
    compute_pipeline_factory = std::make_unique<ComputePipelineFactory>(d);
  }

  std::unordered_map<std::uint32_t, BindingSet> binds;
  std::vector<VkPushConstantRange> push_constants;
  string_hash_map<std::tuple<std::uint32_t, std::uint32_t>> new_binding_info;

  for (const auto& stage : blueprint.shader_stages) {
    if (!stage.empty)
      reflect_shader_using_spirv_reflect(
        stage.filepath, binds, push_constants, new_binding_info);
  }

  std::vector<VkDescriptorSetLayout> all_layouts(binds.size(), VK_NULL_HANDLE);
  for (auto& [set, binding_set] : binds) {
    std::vector<VkDescriptorSetLayoutBinding> binding_vec{ binding_set.begin(),
                                                           binding_set.end() };

    VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(binding_vec.size()),
      .pBindings = binding_vec.data(),
    };

    if (vkCreateDescriptorSetLayout(
          d.get_device(), &layout_info, nullptr, &all_layouts.at(set)) !=
        VK_SUCCESS)
      return false;
  }

  frame_array<VkDescriptorSet> new_descriptor_sets{};
  new_descriptor_sets.fill(VK_NULL_HANDLE);
  VkDescriptorPool new_pool{ VK_NULL_HANDLE };

  std::vector<VkDescriptorSetLayout> material_sets;
  if (binds.contains(0))
    material_sets.push_back(all_layouts.at(0));
  if (binds.contains(1)) {
    std::unordered_map<VkDescriptorType, uint32_t> type_counts;
    for (const auto& b : binds.at(1))
      type_counts[b.descriptorType] += b.descriptorCount;

    std::vector<VkDescriptorPoolSize> pool_sizes;
    for (const auto& [type, count] : type_counts)
      pool_sizes.push_back(
        { .type = type, .descriptorCount = count * frames_in_flight });

    VkDescriptorPoolCreateInfo pool_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .maxSets = frames_in_flight,
      .poolSizeCount = static_cast<uint32_t>(pool_sizes.size()),
      .pPoolSizes = pool_sizes.data(),
    };

    if (vkCreateDescriptorPool(
          d.get_device(), &pool_info, nullptr, &new_pool) != VK_SUCCESS)
      return false;

    std::vector repeated_layouts(frames_in_flight, all_layouts.at(1));
    VkDescriptorSetAllocateInfo alloc_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = new_pool,
      .descriptorSetCount = frames_in_flight,
      .pSetLayouts = repeated_layouts.data(),
    };

    if (vkAllocateDescriptorSets(d.get_device(),
                                 &alloc_info,
                                 new_descriptor_sets.data()) != VK_SUCCESS)
      return false;

    material_sets.push_back(all_layouts.at(1));
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

  const auto* factory =
    is_compute ? compute_pipeline_factory.get() : pipeline_factory.get();

  auto pipeline_result = factory->create_pipeline(
    blueprint,
    PipelineLayoutInfo{ .material_sets = material_sets,
                        .push_constants = push_constants });

  if (!pipeline_result)
    return false;

  // destroy(); // cleanup old Vulkan resources

  binding_info = std::move(new_binding_info);
  descriptor_sets = new_descriptor_sets;
  descriptor_set_layouts = std::move(all_layouts);
  descriptor_pool = new_pool;
  pipeline = std::move(*pipeline_result);
  pipeline_hash = blueprint.hash();
  upload_default_textures();

  return true;
}

auto
Material::invalidate(const std::span<const Image*> images) -> void
{
  for (auto* img : images) {
    for (const auto& [key, binding] : bindings) {
      const auto* img_binding = dynamic_cast<ImageBinding*>(binding.get());
      if (!img_binding) {
        continue;
      }
      if (img_binding->get_underlying_image() != img) {
        continue;
      }

      for (auto& dirty_set : per_frame_dirty_flags) {
        dirty_set.insert(key);
      }
    }
  }
}

auto
Material::reload(const PipelineBlueprint& blueprint) -> void
{
  const auto new_hash = blueprint.hash();
  if (new_hash != 0 && new_hash == pipeline_hash) {
    std::cout << "Pipeline already up to date\n";
    return;
  }

  auto rebuilt_result = rebuild_pipeline(blueprint);
  if (!rebuilt_result) {
    Logger::log_error("Failed to rebuild pipeline: {}",
                      rebuilt_result.error().message);
    return;
  }

  Logger::log_info("Rebuilt pipeline: {}", blueprint.name);
  pipeline = std::move(*rebuilt_result);
  pipeline_hash = new_hash;
}

Material::Material(
  const Device& dev,
  frame_array<VkDescriptorSet>&& sets,
  std::vector<VkDescriptorSetLayout>&& set_layouts,
  const VkDescriptorPool pool,
  std::unique_ptr<CompiledPipeline> pipe,
  string_hash_map<std::tuple<std::uint32_t, std::uint32_t>>&& binds)
  : binding_info(std::move(binds))
  , device(&dev)
  , descriptor_sets(std::move(sets))
  , descriptor_set_layouts(std::move(set_layouts))
  , descriptor_pool(pool)
  , pipeline(std::move(pipe))
{
  upload_default_textures();
}

auto
Material::rebuild_pipeline(const PipelineBlueprint& blueprint)
  -> std::expected<std::unique_ptr<CompiledPipeline>, MaterialError>
{
  std::unordered_map<std::uint32_t, BindingSet> binds;
  std::vector<VkPushConstantRange> push_constants;
  binding_info.clear();

  for (const auto& stage : blueprint.shader_stages) {
    if (!stage.empty)
      reflect_shader_using_spirv_reflect(
        stage.filepath, binds, push_constants, binding_info);
  }

  std::vector<VkDescriptorSetLayout> all_layouts;
  all_layouts.resize(binds.size());

  std::vector<VkDescriptorSetLayout> new_layouts;

  std::size_t i = 0;
  for (auto& b : binds | std::views::values) {
    std::vector<VkDescriptorSetLayoutBinding> new_bindings{ b.begin(),
                                                            b.end() };

    VkDescriptorSetLayoutCreateInfo layout_info{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<std::uint32_t>(new_bindings.size()),
      .pBindings = new_bindings.data(),
    };

    VkDescriptorSetLayout layout;
    if (vkCreateDescriptorSetLayout(
          device->get_device(), &layout_info, nullptr, &layout) != VK_SUCCESS)
      return std::unexpected(MaterialError{
        .message = "Failed to create descriptor set layout in rebuild_pipeline",
        .code = MaterialError::Code::descriptor_layout_failed,
      });
    all_layouts[i++] = layout;
    new_layouts.push_back(layout);
  }

  std::vector<VkDescriptorSetLayout> material_sets;
  if (binds.contains(0)) {
    material_sets.push_back(all_layouts[0]);
  }
  if (binds.contains(1)) {
    material_sets.push_back(all_layouts[1]);
  }

  const bool is_compute =
    blueprint.shader_stages.size() == 1 &&
    blueprint.shader_stages[0].stage == ShaderStage::compute;

  const auto* factory =
    is_compute ? compute_pipeline_factory.get() : pipeline_factory.get();

  auto result = factory->create_pipeline(blueprint,
                                         PipelineLayoutInfo{
                                           .material_sets = material_sets,
                                           .push_constants = push_constants,
                                         });

  if (!result) {
    for (const auto layout : new_layouts) {
      vkDestroyDescriptorSetLayout(device->get_device(), layout, nullptr);
    }

    return std::unexpected(MaterialError{
      .message = "Failed to rebuild pipeline: " + result.error().message,
      .code = MaterialError::Code::pipeline_error,
      .inner_pipeline_error = result.error(),
    });
  }

  for (const auto& layout : descriptor_set_layouts) {
    vkDestroyDescriptorSetLayout(device->get_device(), layout, nullptr);
  }

  // Store the new layouts
  descriptor_set_layouts = std::move(new_layouts);

  return std::move(*result);
}

auto
Material::upload_default_textures() -> void
{
  using namespace std::string_view_literals;
  static constexpr auto names = std::array{
    "albedo_map"sv,   "normal_map"sv, "roughness_map"sv,
    "metallic_map"sv, "ao_map"sv,     "emissive_map"sv,
  };

  for (const auto& name : names) {
    if (binding_info.contains(name)) {
      upload(name, Renderer::get_white_texture());
    }
  }
}

auto
Material::upload(const std::string_view name, const GPUBuffer* buffer) -> void
{
  const auto key = std::string(name);
  const auto it = bindings.find(key);

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
    if (const auto* existing = dynamic_cast<BufferBinding*>(it->second.get());
        existing && existing->get_underlying_buffer() == buffer) {
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
Material::upload(const std::string_view name, const Image* image) -> void
{
  if (image == nullptr)
    return;

  if (image->is_used_as_storage()) {
    return upload_storage_image(name, image);
  }

  const auto key = std::string(name);
  const auto it = bindings.find(key);

  const auto binding_it = binding_info.find(key);
  if (binding_it == binding_info.end())
    return;

  const uint32_t binding_index = std::get<1>(binding_it->second);

  bool needs_update = true;
  if (it != bindings.end()) {
    if (const auto* existing = dynamic_cast<ImageBinding*>(it->second.get());
        existing && existing->get_underlying_image() == image) {
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
Material::upload_storage_image(const std::string_view name, const Image* image)
  -> void
{
  if (image == nullptr)
    return;

  const auto key = std::string(name);
  const auto it = bindings.find(key);

  const auto binding_it = binding_info.find(key);
  if (binding_it == binding_info.end())
    return;

  const uint32_t binding_index = std::get<1>(binding_it->second);

  bool needs_update = true;
  if (it != bindings.end()) {
    if (const auto* existing = dynamic_cast<ImageBinding*>(it->second.get());
        existing && existing->get_underlying_image() == image) {
      needs_update = false;
    }
  }

  if (needs_update) {
    bindings[key] = std::make_unique<ImageBinding>(
      image, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, binding_index);
    for (auto& dirty : per_frame_dirty_flags) {
      dirty.insert(key);
    }
  }
}

auto
Material::prepare_for_rendering(const std::uint32_t frame_index)
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
auto
Material::generate_push_constant_data() const -> PushConstantInformation
{
  return {
    .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
    .offset = 0,
    .size = static_cast<std::uint32_t>(sizeof(MaterialData)),
    .pointer = &material_data,
  };
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
  for (const auto& layout : descriptor_set_layouts)
    vkDestroyDescriptorSetLayout(device->get_device(), layout, nullptr);
  descriptor_sets.fill(VK_NULL_HANDLE);

  pipeline.reset();
}