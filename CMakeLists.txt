# 指定此專案最低需要 CMake 3.21
cmake_minimum_required (VERSION 3.21)

# 如果目前 CMake 支援 CMP0162 Policy，則使用新版行為
if(POLICY CMP0162)
   cmake_policy(SET CMP0162 NEW)
endif()

# 建立 SkylineEngine 專案，設定描述、使用 C++，並指定版本為 1.0.0
project (SkylineEngine
  DESCRIPTION "CMake example for Direct3D 12 Game (Win32) w/ DeviceResources using VCPKG"
  LANGUAGES CXX
  VERSION 1.0.0)

# 提供是否建立測試範本的選項，預設關閉
option(BUILD_TEST_TEMPLATE "Ignore warnings related to TODOs" OFF)

# 提供是否在建置時啟用靜態程式碼分析的選項，預設關閉
option(ENABLE_CODE_ANALYSIS "Use Static Code Analysis on build" OFF)

option(SKYLINE_ENABLE_TRACY "Enable Tracy CPU and D3D12 GPU profiling" OFF)

# 指定專案使用 C++17。
set(CMAKE_CXX_STANDARD 17)

# 強制要求編譯器必須支援指定的 C++17 標準
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 關閉編譯器特有的 C++ 擴充功能，盡量使用標準 C++
set(CMAKE_CXX_EXTENSIONS OFF)

# 指定靜態 Library 的輸出目錄為 build/lib
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 指定動態 Library 的輸出目錄為 build/lib
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# 指定執行檔與 Windows DLL 等 Runtime 檔案的輸出目錄為 build/bin
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 載入自訂的 Compiler 與 Linker 相關 CMake 設定
include(CompilerAndLinker.cmake)

# 所有範例與測試的 POST_BUILD 都會把 DLL 與 assets 複製到同一個 bin/ 目錄。
# Ninja 會平行連結多個 Target，同時對同一個目錄做 copy_directory 會互相搶檔案，
# 造成間歇性的 "Error copying directory ... Permission denied"。
# 這裡建立深度為 1 的 Job Pool，讓這些「連結 + 複製」步驟依序執行（僅 Ninja 生效，其他產生器會忽略）。
set_property(GLOBAL APPEND PROPERTY JOB_POOLS skyline_shared_output=1)

find_package(directxmath CONFIG REQUIRED)
find_package(directx-headers CONFIG REQUIRED)
find_package(directx-dxc CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)

find_package(assimp CONFIG REQUIRED)

# 指定 gflags 使用帶有 Namespace 的 CMake Target 名稱
set(GFLAGS_USE_TARGET_NAMESPACE ON)

find_package(gflags CONFIG REQUIRED)

find_package(glog CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)

if(SKYLINE_ENABLE_TRACY)
   find_package(Tracy CONFIG REQUIRED)
endif()

# 定義一個函式，在指定 Target 建置完成後自動複製其執行時需要的 DLL
function(copy_runtime_dependencies target)
   # POST_BUILD 屬於連結步驟，放進同一個 Pool 以避免多個 Target 同時寫入 bin/
   set_property(TARGET ${target} PROPERTY JOB_POOL_LINK skyline_shared_output)

   add_custom_command(
           TARGET ${target}
           POST_BUILD
           COMMAND ${CMAKE_COMMAND} -E copy_if_different
           $<TARGET_RUNTIME_DLLS:${target}>
           $<TARGET_FILE_DIR:${target}>
           COMMAND_EXPAND_LISTS
           COMMENT "Copying runtime dependencies for ${target}"
   )

   add_custom_command(
           TARGET ${target}
           POST_BUILD
           COMMAND ${CMAKE_COMMAND} -E copy_directory
           "${CMAKE_SOURCE_DIR}/MyDXEngineSDK/shaders"
           "$<TARGET_FILE_DIR:${target}>/assets/shaders"
           COMMENT "Copying MyDXEngineSDK shaders for ${target}"
   )
endfunction()

# 使用 add_subdirectory 來把各個資料夾底下的內容設定成子專案並處理其中的 CMakeLists.txt
add_subdirectory(MyDXEngineSDK)
add_subdirectory(iTHelpContest30/DayX_Triangle)
add_subdirectory(iTHelpContest30/DayX_Sky)
add_subdirectory(iTHelpContest30/DayX_Light)
add_subdirectory(iTHelpContest30/DayX_Pass)
add_subdirectory(iTHelpContest30/DayX_MVP)
add_subdirectory(iTHelpContest30/DayX_Cloud)
add_subdirectory(iTHelpContest30/DayX_ColorBlock)
add_subdirectory(iTHelpContest30/dayx_block_select)
add_subdirectory(iTHelpContest30/DayX_ImGui)
add_subdirectory(iTHelpContest30/DayX_ComputeShader)
add_subdirectory(iTHelpContest30/DayX_Fbm)
add_subdirectory(iTHelpContest30/DayX_FBX)
add_subdirectory(iTHelpContest30/DayX_PBR)
add_subdirectory(iTHelpContest30/DayX_Raymarching)
add_subdirectory(iTHelpContest30/dayx_texture)

include(CTest)

if(BUILD_TESTING)
   find_package(Catch2 3 CONFIG REQUIRED)

   include(Catch)
   if(EXISTS "${CMAKE_SOURCE_DIR}/MyDXEngineSDK/tests/SmokeTest.cpp")
      add_executable(SkylineTests MyDXEngineSDK/tests/SmokeTest.cpp)
      target_link_libraries(SkylineTests PRIVATE Catch2::Catch2WithMain)
      catch_discover_tests(SkylineTests)
   endif()
   add_subdirectory(MyDXEngineSDK/tests/math)
   add_subdirectory(MyDXEngineSDK/tests/render)
   add_subdirectory(MyDXEngineSDK/tests/rhi)
endif()
