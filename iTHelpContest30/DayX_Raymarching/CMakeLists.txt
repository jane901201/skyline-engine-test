cmake_minimum_required(VERSION 3.25)

project(DayX_Raymarching LANGUAGES CXX)

add_executable(DayX_Raymarching WIN32 main.cpp)
target_compile_features(DayX_Raymarching PRIVATE cxx_std_20)
target_link_libraries(DayX_Raymarching PRIVATE MyDXEngineSDK::MyDXEngineSDK)

copy_runtime_dependencies(DayX_Raymarching)

set_target_properties(DayX_Raymarching PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Raymarching>"
)

add_custom_command(TARGET DayX_Raymarching POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Raymarching>/assets"
)
