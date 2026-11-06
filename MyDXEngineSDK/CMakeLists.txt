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
        audio/audio_types.h
        audio/audio_device.h
        audio/audio_device.cpp
        backends/miniaudio/miniaudio_device.h
        backends/miniaudio/miniaudio_device.cpp
        input/input_types.h
        input/input_state.h
        input/input_device.h
        input/input_device.cpp
        backends/win32/win32_input_device.h
        backends/win32/win32_input_device.cpp
        physics/physics_types.h
        physics/physics_world.h
        physics/physics_world.cpp
        scene/transform.h
        scene/camera.h
        scene/scene.h
        scene/fly_camera.h
        scene/euler.h
        gameplay/fly_camera_behaviour.h
        gameplay/animator_behaviour.h
        gameplay/rigid_body_behaviour.h
        gameplay/game_time.h
        gameplay/behaviour.h
        gameplay/game_world.h
        animation/animation_clip.h
        animation/animator.h
        animation/skeleton.h
        backends/jolt/jolt_physics_world.h
        core/skyline_api.h
        $<TARGET_OBJECTS:SkylineJoltBackend>
)

target_compile_definitions(MyDXEngineSDK PRIVATE MYDXENGINESDK_BUILD)

# Jolt 後端獨立成 Object Library，原因：
# 1. Jolt 的 vcpkg 套件以 AVX2 編譯，使用端必須用相同指令集（否則 RegisterTypes 會中止程式），
#    把 /arch:AVX2 限制在這個 Target，不會影響其他引擎程式碼與使用 SDK 的範例。
# 2. Jolt 的標頭與型別不會外洩到 SDK 的公開標頭。
# 它的物件檔以 $<TARGET_OBJECTS:> 併入 MyDXEngineSDK（Object Library 定義在檔案最後）。
# Jolt.lib 只以 LINK_ONLY 連結，避免它的編譯選項與定義傳遞給 MyDXEngineSDK。
target_link_libraries(MyDXEngineSDK PRIVATE $<LINK_ONLY:Jolt::Jolt>)

# miniaudio.h 只在 miniaudio 後端的 .cpp 內使用，不需要暴露給使用 SDK 的 Target。
target_include_directories(MyDXEngineSDK PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

# Win32 輸入後端使用 SetWindowSubclass，需要連結 comctl32。
target_link_libraries(MyDXEngineSDK PRIVATE comctl32)

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

# Jolt Physics 後端：只編譯成物件檔併入 MyDXEngineSDK.dll（見上方說明）。
add_library(SkylineJoltBackend OBJECT
        backends/jolt/jolt_physics_world.h
        backends/jolt/jolt_physics_world.cpp
)

# 後端的函式要從 DLL 匯出，所以需要和 MyDXEngineSDK 相同的匯出定義
target_compile_definitions(SkylineJoltBackend PRIVATE MYDXENGINESDK_BUILD)
target_compile_features(SkylineJoltBackend PRIVATE cxx_std_20)
target_include_directories(SkylineJoltBackend PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/core
)
target_link_libraries(SkylineJoltBackend PRIVATE Jolt::Jolt Microsoft::DirectXMath)

# Jolt 的 vcpkg 套件預設以 AVX2 編譯，使用端必須使用相同的指令集設定
if(MSVC)
        target_compile_options(SkylineJoltBackend PRIVATE /arch:AVX2)
endif()
