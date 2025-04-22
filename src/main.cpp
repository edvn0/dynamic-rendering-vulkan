#include <array>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <memory>
#include <thread>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

#include "GLFW/glfw3.h"
#include "VkBootstrap.h"

static PFN_vkCmdBeginRenderingKHR vkCmdBeginRenderingKHRCall = nullptr;
static PFN_vkCmdEndRenderingKHR vkCmdEndRenderingKHRCall = nullptr;

class Instance {
public:
  static auto create() -> Instance {
    vkb::InstanceBuilder builder;
    auto result = builder.set_app_name("Example Vulkan Application")
                      .request_validation_layers()
                      .require_api_version(1, 3)
                      .use_default_debug_messenger()
                      .build();
    if (!result) {
      std::cerr << "Failed to create Vulkan instance: "
                << result.error().message() << "\n";
      assert(false && "Failed to create Vulkan instance");
    }
    return Instance(result.value());
  }

  auto raw() const -> VkInstance { return instance.instance; }

  auto vkb() const -> const vkb::Instance & { return instance; }

private:
  explicit Instance(const vkb::Instance &inst) : instance(inst) {}

  vkb::Instance instance;
};

class Window;
static auto framebuffer_resize_callback(GLFWwindow *window, int, int) -> void;

class Window {
public:
  Window() {
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WIN32);

    if (glfwInit() == GLFW_FALSE) {
      assert(false && "Failed to initialize GLFW");
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    glfw_window =
        glfwCreateWindow(1280, 720, "Vulkan Window", nullptr, nullptr);
    if (!glfw_window) {
      glfwTerminate();
      assert(false && "Failed to create GLFW window");
    }

    glfwSetWindowUserPointer(glfw_window, this);
    glfwSetFramebufferSizeCallback(glfw_window, framebuffer_resize_callback);
  }

  void create_surface(const Instance &instance) {
    if (glfwCreateWindowSurface(instance.raw(), glfw_window, nullptr,
                                &vk_surface) != VK_SUCCESS) {
      glfwDestroyWindow(glfw_window);
      glfwTerminate();
      assert(false && "Failed to create window surface");
    }
  }
  auto window() const -> const auto * { return glfw_window; }
  auto window() -> auto * { return glfw_window; }
  auto surface() const -> VkSurfaceKHR { return vk_surface; }
  auto framebuffer_resized() const -> bool { return framebuffer_was_resized; }
  auto set_resize_flag(bool flag) -> void { framebuffer_was_resized = flag; }
  auto should_close() const -> bool {
    return glfwWindowShouldClose(glfw_window);
  }
  auto is_iconified() const -> bool {
    return glfwGetWindowAttrib(glfw_window, GLFW_ICONIFIED) != 0;
  }

  ~Window() {
    if (glfw_window) {
      glfwDestroyWindow(glfw_window);
    }
    glfwTerminate();
  }

private:
  GLFWwindow *glfw_window{nullptr};
  VkSurfaceKHR vk_surface{VK_NULL_HANDLE};
  bool framebuffer_was_resized{false};
};

class Device {
public:
  static auto create(const Instance &instance, const VkSurfaceKHR &surface)
      -> Device {
    vkb::PhysicalDeviceSelector selector{instance.vkb()};
    auto phys_result = selector.set_surface(surface)
                           .set_minimum_version(1, 3)
                           .require_dedicated_transfer_queue()
                           .select();
    if (!phys_result) {
      assert(false && "Failed to select physical device");
    }

    vkb::DeviceBuilder builder{phys_result.value()};
    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering{
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .pNext = nullptr,
        .dynamicRendering = VK_TRUE,
    };

    builder.add_pNext(&dynamic_rendering);
    auto dev_result = builder.build();
    if (!dev_result) {
      assert(false && "Failed to create device");
    }

    return Device(dev_result.value());
  }

  auto graphics_queue() const -> VkQueue {
    auto queue_result = device.get_queue(vkb::QueueType::graphics);
    if (!queue_result) {
      assert(false && "Failed to get graphics queue");
    }
    return queue_result.value();
  }

  auto get_device() const -> VkDevice { return device.device; }

  auto get_physical_device() const -> VkPhysicalDevice {
    return device.physical_device.physical_device;
  }

  auto graphics_queue_family_index() const -> uint32_t {
    auto queue_result = device.get_queue_index(vkb::QueueType::graphics);
    if (!queue_result) {
      assert(false && "Failed to get graphics queue family index");
    }
    return queue_result.value();
  }

private:
  explicit Device(const vkb::Device &dev) : device(dev) {}

  vkb::Device device;
};

struct FrametimeCalculator {
  using clock = std::chrono::high_resolution_clock;

  auto start() { start_time = clock::now(); }

  auto end_and_get_delta_ms() const -> double {
    auto end_time = clock::now();
    auto delta =
        std::chrono::duration<double, std::milli>(end_time - start_time);
    return delta.count();
  }

private:
  clock::time_point start_time;
};

class GUISystem {
public:
  explicit GUISystem(const Instance &instance, const Device &device,
                     Window &window) {
    initialise_for_vulkan(instance, device, window);
  }

  ~GUISystem() { shutdown(); }

  auto begin_frame() const {
    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
  }

  auto end_frame(VkCommandBuffer command_buffer) const -> void {
    ImGui::Render();
    ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command_buffer);
  }

  auto draw() const -> void {
    static bool show_demo_window = true;
    ImGui::ShowDemoWindow(&show_demo_window);
  }

private:
  auto initialise_for_vulkan(const Instance &instance, const Device &device,
                             Window &window) const -> void {
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForVulkan(window.window(), true);
    ImGui_ImplVulkan_InitInfo init_info = {};
    init_info.Instance = instance.raw();
    init_info.PhysicalDevice = device.get_physical_device();
    init_info.Device = device.get_device();
    init_info.QueueFamily = device.graphics_queue_family_index();
    init_info.Queue = device.graphics_queue();
    init_info.PipelineCache = VK_NULL_HANDLE;
    init_info.DescriptorPool = VK_NULL_HANDLE;
    init_info.MinImageCount = 2;
    init_info.ImageCount = 2;
    init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
    init_info.Allocator = nullptr;
    init_info.CheckVkResultFn = nullptr;
    init_info.Subpass = 0;
    init_info.DescriptorPoolSize = 5;
    init_info.MinAllocationSize = 1024 * 1024;
    init_info.UseDynamicRendering = true;
    init_info.PipelineRenderingCreateInfo = {};
    init_info.PipelineRenderingCreateInfo.sType =
        VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;

    const std::array formats = {
        VK_FORMAT_B8G8R8A8_UNORM,
    };
    init_info.PipelineRenderingCreateInfo.pColorAttachmentFormats =
        formats.data();
    init_info.PipelineRenderingCreateInfo.colorAttachmentCount =
        static_cast<uint32_t>(formats.size());
    init_info.PipelineRenderingCreateInfo.viewMask = 0;

    ImGui_ImplVulkan_Init(&init_info);
  }

  auto shutdown() const -> void {
    ImGui_ImplVulkan_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
  }
};
class Swapchain {
public:
  static constexpr uint32_t image_count = 2;

  Swapchain(const Device &device, const Window &window)
      : device_(device.get_device()),
        physical_device_(device.get_physical_device()),
        surface_(window.surface()), queue_(device.graphics_queue()),
        queue_family_index_(device.graphics_queue_family_index()) {
    create_swapchain();
    create_command_pool();
    allocate_command_buffers();
    create_sync_objects();
  }

  ~Swapchain() {
    cleanup_swapchain();
    vkDestroyCommandPool(device_, command_pool_, nullptr);
    for (uint32_t i = 0; i < image_count; ++i) {
      vkDestroySemaphore(device_, render_finished_semaphores_[i], nullptr);
      vkDestroySemaphore(device_, image_available_semaphores_[i], nullptr);
      vkDestroyFence(device_, in_flight_fences_[i], nullptr);
    }
  }

  void request_recreate(Window &w) { recreate_swapchain(w); }

  auto draw_frame(Window &w, GUISystem &gui_system) -> void {
    vkWaitForFences(device_, 1, &in_flight_fences_[frame_index_], VK_TRUE,
                    UINT64_MAX);
    vkResetFences(device_, 1, &in_flight_fences_[frame_index_]);

    uint32_t image_index;
    auto result =
        vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX,
                              image_available_semaphores_[frame_index_],
                              VK_NULL_HANDLE, &image_index);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
      recreate_swapchain(w);
      return;
    }

    VkCommandBuffer cmd = command_buffers_[frame_index_];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cmd, &begin_info);

    const VkImageMemoryBarrier image_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = 0,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .srcQueueFamilyIndex = queue_family_index_,
        .dstQueueFamilyIndex = queue_family_index_,
        .image = swapchain_images_[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,             // srcStageMask
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                    // imageMemoryBarrierCount
        &image_memory_barrier // pImageMemoryBarriers
    );

    const VkRenderingAttachmentInfo color_attachment_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .pNext = nullptr,
        .imageView = this->swapchain_image_views_[image_index],
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .resolveMode = VK_RESOLVE_MODE_NONE_KHR,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue =
            {
                .color = {{0.0f, 0.0f, 0.0f, 1.0f}},
            },
    };

    const VkRenderingInfo render_info{
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR,
        .pNext = nullptr,
        .flags = 0,
        .renderArea =
            {
                .offset = {0, 0},
                .extent = swapchain_extent_,
            },
        .layerCount = 1,
        .viewMask = 0,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
        .pDepthAttachment = nullptr,
        .pStencilAttachment = nullptr,
    };

    vkCmdBeginRendering(cmd, &render_info);

    gui_system.draw();
    gui_system.end_frame(cmd);

    vkCmdEndRendering(cmd);

    const VkImageMemoryBarrier post_rendering_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .pNext = nullptr,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = queue_family_index_,
        .dstQueueFamilyIndex = queue_family_index_,
        .image = swapchain_images_[image_index],
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    vkCmdPipelineBarrier(
        cmd,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, // srcStageMask
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,          // dstStageMask
        0, 0, nullptr, 0, nullptr,
        1,                      // imageMemoryBarrierCount
        &post_rendering_barrier // pImageMemoryBarriers
    );

    vkEndCommandBuffer(cmd);

    const std::array buffers = {cmd};
    const std::array wait_semaphores = {
        image_available_semaphores_[frame_index_],
    };
    const std::array signal_semaphores = {
        render_finished_semaphores_[frame_index_],
    };
    const std::array<VkPipelineStageFlags, 1> wait_stages = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount =
        static_cast<uint32_t>(wait_semaphores.size());
    submit_info.pWaitSemaphores = wait_semaphores.data();
    submit_info.pWaitDstStageMask = wait_stages.data();
    submit_info.commandBufferCount = static_cast<uint32_t>(buffers.size());
    submit_info.pCommandBuffers = buffers.data();
    submit_info.signalSemaphoreCount =
        static_cast<uint32_t>(signal_semaphores.size());
    submit_info.pSignalSemaphores = signal_semaphores.data();

    vkQueueSubmit(queue_, 1, &submit_info, in_flight_fences_[frame_index_]);

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_finished_semaphores_[frame_index_];
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain_;
    present_info.pImageIndices = &image_index;

    result = vkQueuePresentKHR(queue_, &present_info);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      recreate_swapchain(w);
    }

    frame_index_ = (frame_index_ + 1) % image_count;
  }

private:
  VkDevice device_;
  VkPhysicalDevice physical_device_;
  VkSurfaceKHR surface_;
  VkQueue queue_;
  uint32_t queue_family_index_;
  uint32_t frame_index_ = 0;

  VkSwapchainKHR swapchain_{VK_NULL_HANDLE};
  VkFormat swapchain_format_;
  VkExtent2D swapchain_extent_;

  std::vector<VkImage> swapchain_images_;
  std::vector<VkImageView> swapchain_image_views_;

  VkCommandPool command_pool_{VK_NULL_HANDLE};
  std::array<VkCommandBuffer, image_count> command_buffers_{};

  std::array<VkSemaphore, image_count> image_available_semaphores_{};
  std::array<VkSemaphore, image_count> render_finished_semaphores_{};
  std::array<VkFence, image_count> in_flight_fences_{};

  void create_swapchain() {
    VkSurfaceCapabilitiesKHR caps;
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device_, surface_,
                                              &caps);

    swapchain_extent_ = caps.currentExtent;
    swapchain_format_ = VK_FORMAT_B8G8R8A8_UNORM;

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_,
                                         &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device_, surface_,
                                         &format_count, formats.data());

    if (formats[0].format != VK_FORMAT_UNDEFINED) {
      swapchain_format_ = formats[0].format;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = surface_;
    create_info.minImageCount = image_count;
    create_info.imageFormat = swapchain_format_;
    create_info.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    create_info.imageExtent = swapchain_extent_;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create_info.preTransform = caps.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create_info.clipped = VK_TRUE;

    vkCreateSwapchainKHR(device_, &create_info, nullptr, &swapchain_);

    uint32_t count = 0;
    vkGetSwapchainImagesKHR(device_, swapchain_, &count, nullptr);
    swapchain_images_.resize(count);
    vkGetSwapchainImagesKHR(device_, swapchain_, &count,
                            swapchain_images_.data());

    swapchain_image_views_.resize(count);
    for (size_t i = 0; i < count; ++i) {
      VkImageViewCreateInfo view_info{};
      view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
      view_info.image = swapchain_images_[i];
      view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
      view_info.format = swapchain_format_;
      view_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
      view_info.subresourceRange.levelCount = 1;
      view_info.subresourceRange.layerCount = 1;

      vkCreateImageView(device_, &view_info, nullptr,
                        &swapchain_image_views_[i]);
    }
  }

  void recreate_swapchain(Window &window) {
    int width = 0;
    int height = 0;
    while (width == 0 || height == 0) {
      glfwGetFramebufferSize(window.window(), &width, &height);
      glfwWaitEvents();
    }

    vkDeviceWaitIdle(device_);
    cleanup_swapchain();
    create_swapchain();
  }

  void cleanup_swapchain() {
    for (auto view : swapchain_image_views_) {
      vkDestroyImageView(device_, view, nullptr);
    }
    vkDestroySwapchainKHR(device_, swapchain_, nullptr);
  }

  void create_command_pool() {
    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_index_;
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    vkCreateCommandPool(device_, &pool_info, nullptr, &command_pool_);
  }

  void allocate_command_buffers() {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = command_pool_;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = image_count;
    vkAllocateCommandBuffers(device_, &alloc_info, command_buffers_.data());
  }

  void create_sync_objects() {
    VkSemaphoreCreateInfo semaphore_info{
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
        .pNext = nullptr,
        .flags = 0,
    };
    VkFenceCreateInfo fence_info{
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .pNext = nullptr,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (uint32_t i = 0; i < image_count; ++i) {
      vkCreateSemaphore(device_, &semaphore_info, nullptr,
                        &image_available_semaphores_[i]);
      vkCreateSemaphore(device_, &semaphore_info, nullptr,
                        &render_finished_semaphores_[i]);
      vkCreateFence(device_, &fence_info, nullptr, &in_flight_fences_[i]);
    }
  }
};

auto main(int, char **) -> std::int32_t {
  auto instance = Instance::create();
  Window window;
  window.create_surface(instance);
  auto device = Device::create(instance, window.surface());

  vkCmdBeginRenderingKHRCall = std::bit_cast<PFN_vkCmdBeginRenderingKHR>(
      vkGetInstanceProcAddr(instance.raw(), "vkCmdBeginRenderingKHR"));
  vkCmdEndRenderingKHRCall = std::bit_cast<PFN_vkCmdEndRenderingKHR>(
      vkGetInstanceProcAddr(instance.raw(), "vkCmdEndRenderingKHR"));

  GUISystem gui_system(instance, device, window);
  Swapchain swapchain(device, window);

  FrametimeCalculator timer;
  while (!window.should_close()) {
    if (window.is_iconified()) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
      continue;
    }

    timer.start();
    glfwPollEvents();

    if (window.framebuffer_resized()) {
      swapchain.request_recreate(window);
      window.set_resize_flag(false);
    }

    gui_system.begin_frame();
    swapchain.draw_frame(window, gui_system);

    auto frametime_ms = timer.end_and_get_delta_ms();
    std::cout << "Frame time: " << frametime_ms << " ms\n";
  }

  vkDeviceWaitIdle(device.get_device());

  return 0;
}

static auto framebuffer_resize_callback(GLFWwindow *window, int, int) -> void {
  auto *self = static_cast<Window *>(glfwGetWindowUserPointer(window));
  self->set_resize_flag(true);
}