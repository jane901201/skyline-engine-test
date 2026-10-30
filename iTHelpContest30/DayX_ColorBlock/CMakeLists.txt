cmake_minimum_required(VERSION 3.25)

project(DayX_ColorBlock LANGUAGES CXX)

add_executable(DayX_ColorBlock WIN32
        main.cpp
)

target_compile_features(DayX_ColorBlock PRIVATE cxx_std_20)

target_link_libraries(DayX_ColorBlock PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_ColorBlock)

set_target_properties(DayX_ColorBlock PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(TARGET DayX_ColorBlock POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_ColorBlock>/assets"
)
