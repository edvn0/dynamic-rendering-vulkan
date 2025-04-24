#include "gui_system.hpp"

#include <array>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "config.hpp"
#include <ImGuizmo.h>

auto
GUISystem::begin_frame() const -> void
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();

  const auto* vp = ImGui::GetMainViewport();

  ImGui::SetNextWindowPos(vp->Pos);
  ImGui::SetNextWindowSize(vp->Size);
  ImGui::SetNextWindowViewport(vp->ID);
  ImGui::SetNextWindowBgAlpha(0.0f);

  ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

  ImGuiWindowFlags host_flags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
    ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus |
    ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoDecoration |
    ImGuiWindowFlags_NoDocking;

  ImGui::Begin("DockSpaceHost", nullptr, host_flags);

  ImGui::PopStyleVar(4);

  ImGuiID dockspace_id = ImGui::GetID("MainDockspace");
  ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_PassthruCentralNode;

  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);

  ImGuizmo::SetOrthographic(false);
  ImGuizmo::BeginFrame();
}

auto
GUISystem::end_frame(VkCommandBuffer cmd_buf) const -> void
{
  ImGui::End();
  ImGui::Render();
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);

  const auto& io = ImGui::GetIO();
  if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
    ImGui::UpdatePlatformWindows();
    ImGui::RenderPlatformWindowsDefault();
  }
}

GUISystem::~GUISystem()
{
  if (!destroyed) {
    shutdown();
  }
}

GUISystem::GUISystem(const Core::Instance& instance,
                     const Device& device,
                     Window& window)
{
  init_for_vulkan(instance, device, window);
}

auto
GUISystem::init_for_vulkan(const Core::Instance& instance,
                           const Device& device,
                           Window& window) const -> void
{
  ImGui::CreateContext();
  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(window.window(), true);

  ImGui_ImplVulkan_InitInfo info{};
  info.Instance = instance.raw();
  info.PhysicalDevice = device.get_physical_device();
  info.Device = device.get_device();
  info.QueueFamily = device.graphics_queue_family_index();
  info.Queue = device.graphics_queue();
  info.PipelineCache = VK_NULL_HANDLE;
  info.DescriptorPool = VK_NULL_HANDLE;
  info.ApiVersion = VK_API_VERSION_1_4;
  info.MinImageCount = image_count;
  info.ImageCount = image_count;
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.DescriptorPoolSize = 100;
  info.Allocator = nullptr;
  info.CheckVkResultFn = +[](VkResult err) {
    if (err == 0)
      return;
    std::cerr << "Vulkan Error: " << err << std::endl;
    assert(false && "Vulkan error");
  };
  info.UseDynamicRendering = true;

  const std::array formats = {
    VK_FORMAT_B8G8R8A8_SRGB,
  };
  info.PipelineRenderingCreateInfo.sType =
    VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
  info.PipelineRenderingCreateInfo.viewMask = 0;
  info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
  info.PipelineRenderingCreateInfo.pColorAttachmentFormats = formats.data();

  ImGui_ImplVulkan_Init(&info);

  // Need to setup dockspace and viewport
  auto& io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
}

auto
GUISystem::shutdown() -> void
{
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  destroyed = true;
}
