cmake_minimum_required(VERSION 3.25)

project(DayX_Fbm LANGUAGES CXX)

add_executable(DayX_Fbm WIN32 main.cpp)
target_compile_features(DayX_Fbm PRIVATE cxx_std_20)
target_link_libraries(DayX_Fbm PRIVATE MyDXEngineSDK::MyDXEngineSDK)

copy_runtime_dependencies(DayX_Fbm)

set_target_properties(DayX_Fbm PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Fbm>"
)

add_custom_command(TARGET DayX_Fbm POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Fbm>/assets"
)
