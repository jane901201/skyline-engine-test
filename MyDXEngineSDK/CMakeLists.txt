# 建立名為 MyDXEngineSDK 的動態函式庫，並指定要編譯進去的原始碼與標頭檔。
add_library(MyDXEngineSDK SHARED
        core/graphics_engine.cpp
        core/tracy_profiler.cpp
        core/tracy_profiler.h
        core/index_buffer.cpp
        core/mesh.h
        core/mesh.cpp
        core/root_signature.cpp
        core/shader.cpp
        core/skyline_debugger.h
        core/skyline_debugger.cpp
        core/vertex_buffer.cpp
        core/constant_buffer.cpp
        core/rw_structured_buffer.h
        core/rw_structured_buffer.cpp
        core/structured_buffer.h
        core/structured_buffer.cpp
        system/system.cpp
        core/pipeline_state.cpp
        rhi/types.h
        rhi/pipeline.h
        rhi/object.h
        rhi/shader.h
        rhi/binding.h
        rhi/device.h
        rhi/device.cpp
        rhi/resource.h
        rhi/command_context.h
        rhi/frame.h
        backends/d3d12/d3d12_resource.cpp
        backends/d3d12/d3d12_texture.cpp
        backends/d3d12/d3d12_command_context.cpp
        backends/d3d12/d3d12_frame.cpp
        backends/d3d12/d3d12_device.cpp
        backends/d3d12/d3d12_shader.cpp
        backends/d3d12/d3d12_binding.cpp
        backends/d3d12/d3d12_interop.cpp
        backends/d3d12/d3d12_pipeline.h
        backends/d3d12/d3d12_pipeline.cpp
        core/render_pass/renderer.cpp
        core/render_pass/render_state.cpp
        core/render_pass/common_pass.cpp
        core/render_pass/cloud_pass.h
        core/render_pass/cloud_pass.cpp
        core/render_pass/light_pass.cpp
        math/vector.cpp
        ui/imgui_system.h
        ui/imgui_system.cpp
        core/skyline_api.h
)

target_compile_definitions(MyDXEngineSDK PRIVATE MYDXENGINESDK_BUILD)

# 為 MyDXEngineSDK 建立命名空間形式的別名，讓其他 Target 可以用 MyDXEngineSDK::MyDXEngineSDK 連結。
add_library(MyDXEngineSDK::MyDXEngineSDK ALIAS MyDXEngineSDK)

# 指定 MyDXEngineSDK 使用 C++20，且這個需求會傳遞給連結它的 Target。
target_compile_features(MyDXEngineSDK PUBLIC cxx_std_20)

# 指定 MyDXEngineSDK 使用 pch.h 作為預編譯標頭，以減少重複編譯時間。
target_precompile_headers(MyDXEngineSDK PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/core/pch.h"
)

# 設定 MyDXEngineSDK 的 Header 搜尋路徑，並讓連結它的 Target 也能使用這些路徑。
target_include_directories(MyDXEngineSDK PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/core
        ${CMAKE_CURRENT_SOURCE_DIR}/system
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core/render_pass
)

# 指定 MyDXEngineSDK 需要連結的 DirectX、Assimp、glog 與 Windows 系統函式庫，並將相依關係傳遞給使用它的 Target。
target_link_libraries(MyDXEngineSDK PUBLIC
        Microsoft::DirectX-Headers
        Microsoft::DirectXMath
        Microsoft::DirectXShaderCompiler
        Microsoft::DirectXTK12
        assimp::assimp
        glog::glog
        d3d12
        d3dcompiler
        dxgi
        dxguid
        user32
        imgui::imgui
)

if(SKYLINE_ENABLE_TRACY)
        target_link_libraries(MyDXEngineSDK PUBLIC Tracy::TracyClient)
        target_compile_definitions(MyDXEngineSDK PUBLIC SKYLINE_ENABLE_TRACY)
endif()
