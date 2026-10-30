cmake_minimum_required(VERSION 3.25)

project(DayX_PBR LANGUAGES CXX)

add_executable(DayX_PBR WIN32 main.cpp)
target_compile_features(DayX_PBR PRIVATE cxx_std_20)
target_link_libraries(DayX_PBR PRIVATE MyDXEngineSDK::MyDXEngineSDK)

copy_runtime_dependencies(DayX_PBR)

set_target_properties(DayX_PBR PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_PBR>"
)

add_custom_command(TARGET DayX_PBR POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_PBR>/assets"
)
