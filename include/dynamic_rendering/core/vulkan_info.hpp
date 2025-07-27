#pragma once

#include <concepts>
#include <span>
#include <vector>
#include <vulkan/vulkan.h>

namespace detail {

// Trait system to define what methods each structure supports
template<typename T>
struct vulkan_traits
{
  static constexpr bool has_command_buffers = false;
  static constexpr bool has_semaphores = false;
  static constexpr bool has_buffer_properties = false;
  static constexpr bool has_image_properties = false;
  static constexpr VkStructureType structure_type = VK_STRUCTURE_TYPE_MAX_ENUM;
};

// Specialize traits for each structure type
template<>
struct vulkan_traits<VkSubmitInfo>
{
  static constexpr bool has_command_buffers = true;
  static constexpr bool has_semaphores = true;
  static constexpr bool has_buffer_properties = false;
  static constexpr bool has_image_properties = false;
  static constexpr VkStructureType structure_type =
    VK_STRUCTURE_TYPE_SUBMIT_INFO;
};

template<>
struct vulkan_traits<VkBufferCreateInfo>
{
  static constexpr bool has_command_buffers = false;
  static constexpr bool has_semaphores = false;
  static constexpr bool has_buffer_properties = true;
  static constexpr bool has_image_properties = false;
  static constexpr VkStructureType structure_type =
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
};

template<>
struct vulkan_traits<VkImageCreateInfo>
{
  static constexpr bool has_command_buffers = false;
  static constexpr bool has_semaphores = false;
  static constexpr bool has_buffer_properties = false;
  static constexpr bool has_image_properties = true;
  static constexpr VkStructureType structure_type =
    VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
};

// Concepts based on traits
template<typename T>
concept has_command_buffers = vulkan_traits<T>::has_command_buffers;

template<typename T>
concept has_semaphores = vulkan_traits<T>::has_semaphores;

template<typename T>
concept has_buffer_properties = vulkan_traits<T>::has_buffer_properties;

template<typename T>
concept has_image_properties = vulkan_traits<T>::has_image_properties;

}

template<typename T>
class info
{
private:
  T data{};

public:
  info()
  {
    data.sType = detail::vulkan_traits<T>::structure_type;
    data.pNext = nullptr;
  }

  T& get() { return data; }
  const T& get() const { return data; }

  operator T&() { return data; }
  operator const T&() const { return data; }

  T* operator->() { return &data; }
  const T* operator->() const { return data; }

  template<std::ranges::contiguous_range R>
    requires detail::has_command_buffers<T>
  auto with_command_buffers(R&& cmd_buffers)
  {
    auto span_cmds = std::span{ cmd_buffers };
    data.commandBufferCount = static_cast<uint32_t>(span_cmds.size());
    data.pCommandBuffers = span_cmds.data();
    return *this;
  }

  info& with_wait_semaphores(std::span<VkSemaphore> semaphores,
                             std::span<VkPipelineStageFlags> stages)
    requires detail::has_semaphores<T>
  {
    data.waitSemaphoreCount = static_cast<uint32_t>(semaphores.size());
    data.pWaitSemaphores = semaphores.data();
    data.pWaitDstStageMask = stages.data();
    return *this;
  }

  info& with_signal_semaphores(std::span<VkSemaphore> semaphores)
    requires detail::has_semaphores<T>
  {
    data.signalSemaphoreCount = static_cast<uint32_t>(semaphores.size());
    data.pSignalSemaphores = semaphores.data();
    return *this;
  }

  info& with_size(VkDeviceSize size)
    requires detail::has_buffer_properties<T>
  {
    data.size = size;
    return *this;
  }

  info& with_usage(VkBufferUsageFlags usage)
    requires detail::has_buffer_properties<T>
  {
    data.usage = usage;
    return *this;
  }

  info& with_sharing_mode(VkSharingMode sharing_mode)
    requires detail::has_buffer_properties<T>
  {
    data.sharingMode = sharing_mode;
    return *this;
  }

  info& with_extent(VkExtent3D extent)
    requires detail::has_image_properties<T>
  {
    data.extent = extent;
    return *this;
  }

  info& with_format(VkFormat format)
    requires detail::has_image_properties<T>
  {
    data.format = format;
    return *this;
  }

  info& with_image_type(VkImageType image_type)
    requires detail::has_image_properties<T>
  {
    data.imageType = image_type;
    return *this;
  }
};
