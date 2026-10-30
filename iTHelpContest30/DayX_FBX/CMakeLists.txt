cmake_minimum_required(VERSION 3.25)

project(DayX_FBX LANGUAGES CXX)

add_executable(DayX_FBX WIN32 main.cpp)
target_compile_features(DayX_FBX PRIVATE cxx_std_20)
target_link_libraries(DayX_FBX PRIVATE MyDXEngineSDK::MyDXEngineSDK)

add_custom_target(DayX_FBX_shader
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DayX_FBX>/assets/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/fbx.hlsl"
        "$<TARGET_FILE_DIR:DayX_FBX>/assets/shaders/fbx.hlsl"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/../assets/shaders/fbx.hlsl"
)
add_dependencies(DayX_FBX DayX_FBX_shader)

copy_runtime_dependencies(DayX_FBX)

set_target_properties(DayX_FBX PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_FBX>"
)

add_custom_command(TARGET DayX_FBX POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_FBX>/assets"
)
