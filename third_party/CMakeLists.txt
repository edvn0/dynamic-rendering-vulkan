add_subdirectory(vk-bootstrap)

function(generate_imgui)
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)

    add_library(imgui STATIC
            ${IMGUI_DIR}/imgui.cpp
            ${IMGUI_DIR}/imgui_draw.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/imgui_tables.cpp
            ${IMGUI_DIR}/imgui_demo.cpp
            ${IMGUI_DIR}/imgui_widgets.cpp
            ${IMGUI_DIR}/imgui_internal.h
            ${IMGUI_DIR}/imconfig.h
            ${IMGUI_DIR}/imstb_rectpack.h
            ${IMGUI_DIR}/imstb_textedit.h
            ${IMGUI_DIR}/imstb_truetype.h

            # Vulkan, GLFW
            ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
            ${IMGUI_DIR}/backends/imgui_impl_glfw.h
            ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
            ${IMGUI_DIR}/backends/imgui_impl_vulkan.h
    )
    target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(imgui PRIVATE Vulkan::Vulkan glfw)
    target_compile_definitions(imgui PUBLIC IMGUI_USE_WCHAR32)
endfunction()

generate_imgui()

function(generate_implot)
    set(IMPLOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/implot)

    add_library(implot STATIC
            ${IMPLOT_DIR}/implot.cpp
            ${IMPLOT_DIR}/implot_items.cpp
            ${IMPLOT_DIR}/implot.h
            ${IMPLOT_DIR}/implot_internal.h
    )
    target_include_directories(implot PUBLIC ${IMPLOT_DIR})
    target_link_libraries(implot PRIVATE imgui)
endfunction()

generate_implot()

function(generate_vma)
    add_library(VulkanMemoryAllocator STATIC ${CMAKE_CURRENT_SOURCE_DIR}/vma.cpp)
    target_include_directories(VulkanMemoryAllocator PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/VulkanMemoryAllocator/include)
    target_link_libraries(VulkanMemoryAllocator PRIVATE Vulkan::Vulkan)
endfunction()

generate_vma()

function(generate_stb)
    add_library(stb STATIC ${CMAKE_CURRENT_SOURCE_DIR}/stb.cpp)
    add_library(stb::stb ALIAS stb)
    target_include_directories(stb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/stb)
endfunction()

generate_stb()

set(SPIRV_REFLECT_EXECUTABLE OFF)
set(SPIRV_REFLECT_STATIC_LIB ON)
add_subdirectory(SPIRV-Reflect)