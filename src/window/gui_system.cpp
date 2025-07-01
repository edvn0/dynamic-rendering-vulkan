#include "window/gui_system.hpp"

#include <array>

#include "core/device.hpp"
#include "core/instance.hpp"
#include "window/swapchain.hpp"
#include "window/window.hpp"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include "core/config.hpp"
#include "core/vulkan_util.hpp"

#include <ImGuizmo.h>
#include <core/fs.hpp>
#include <implot.h>

auto
GUISystem::begin_frame() const -> void
{
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  ImGuizmo::BeginFrame();

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
}

auto
GUISystem::end_frame(VkCommandBuffer cmd_buf) const -> void
{
  ImGui::End();
  ImGui::Render();

  Util::Vulkan::cmd_begin_debug_label(
    cmd_buf, "GUI", { 1.0F, 0.0F, 0.0F, 1.0F });
  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd_buf);
  Util::Vulkan::cmd_end_debug_label(cmd_buf);

  if (const auto& io = ImGui::GetIO();
      io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
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
                     Window& window,
                     Swapchain& sc)
{
  init_for_vulkan(instance, device, window, sc);
}

auto
GUISystem::init_for_vulkan(const Core::Instance& instance,
                           const Device& device,
                           Window& window,
                           Swapchain& swapchain) const -> void
{
  ImGui::CreateContext();
  ImPlot::CreateContext();
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
  info.MinImageCount = swapchain.get_image_count();
  info.ImageCount = swapchain.get_image_count();
  info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
  info.DescriptorPoolSize = 100;
  info.Allocator = nullptr;
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

  const auto roboto_path =
    assets_path() / "fonts" / "roboto" / "static" / "Roboto-Regular.ttf";
  const auto noto_emoji_path =
    assets_path() / "fonts" / "Noto_Color_Emoji" / "NotoColorEmoji-Regular.ttf";

  ImFontConfig roboto_config;
  roboto_config.OversampleH = 3;
  roboto_config.OversampleV = 1;
  roboto_config.PixelSnapH = true;

  io.Fonts->AddFontFromFileTTF(
    roboto_path.string().c_str(), 13.0F, &roboto_config);

  static constexpr ImWchar emoji_ranges[] = {
    0x0020,
    0x00FF, // Basic Latin + Latin-1 Supplement (optional, for ascii & basic
            // symbols)
    0x2000,
    0x206F, // General Punctuation
    0x2190,
    0x21FF, // Arrows
    0x2600,
    0x26FF, // Misc symbols (sun, cloud, snowflake, etc)
    0x2700,
    0x27BF, // Dingbats (checkmarks, crosses, scissors, etc)
    0x1F300,
    0x1F5FF, // Miscellaneous Symbols and Pictographs (emoji part 1)
    0x1F600,
    0x1F64F, // Emoticons (emoji faces)
    0x1F680,
    0x1F6FF, // Transport and Map Symbols
    0x1F700,
    0x1F77F, // Alchemical Symbols (some emojis here)
    0x1F780,
    0x1F7FF, // Geometric Shapes Extended (some emojis)
    0x1F800,
    0x1F8FF, // Supplemental Arrows-C (rarely used emojis)
    0x1F900,
    0x1F9FF, // Supplemental Symbols and Pictographs (emoji part 2)
    0x1FA00,
    0x1FA6F, // Chess Symbols and Symbols and Pictographs Extended-A
    0x1FA70,
    0x1FAFF, // Symbols and Pictographs Extended-B
    0
  };

  // 2. Prepare emoji glyph range
  ImFontGlyphRangesBuilder builder;
  builder.AddText("🌍☀✈❤"); // Add visible emojis (within BMP)
  builder.AddChar(0x1f4c1); // 📁
  builder.AddChar(0x1f4c4); // 📄
  builder.AddChar(0x25c0);  // ◀
  builder.AddChar(0x25b6);  // ▶
  builder.AddChar(0x1f50d); // 🔍
  builder.AddChar(0x1f53c); // 🔎
  builder.AddRanges(
    io.Fonts->GetGlyphRangesDefault()); // Optional base characters
  builder.AddRanges(emoji_ranges);      // Add custom emoji ranges
  ImVector<ImWchar> ranges;
  builder.BuildRanges(&ranges);

  // 3. Load emoji font with MergeMode ON
  ImFontConfig emoji_config;
  emoji_config.MergeMode = true;
  emoji_config.PixelSnapH = true;

  io.Fonts->AddFontFromFileTTF(
    noto_emoji_path.string().c_str(), 13.0F, &emoji_config, ranges.Data);

  // 4. Build atlas
  io.Fonts->Build();
}

auto
GUISystem::shutdown() -> void
{
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  ImPlot::DestroyContext();

  destroyed = true;
}
