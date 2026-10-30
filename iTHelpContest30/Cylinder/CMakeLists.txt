cmake_minimum_required(VERSION 3.25)

project(Cylinder LANGUAGES CXX)

add_executable(Cylinder WIN32
        main.cpp
)

target_compile_features(Cylinder PRIVATE cxx_std_20)

target_link_libraries(Cylinder PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(Cylinder)

set_target_properties(Cylinder PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(TARGET Cylinder POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:Cylinder>/assets"
)