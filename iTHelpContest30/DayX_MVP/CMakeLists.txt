cmake_minimum_required(VERSION 3.25)

project(DayX_MVP LANGUAGES CXX)

add_executable(DayX_MVP WIN32
        main.cpp
)

target_compile_features(DayX_MVP PRIVATE cxx_std_20)

target_link_libraries(DayX_MVP PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_MVP)

set_target_properties(DayX_MVP PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_MVP>"
)

add_custom_command(TARGET DayX_MVP POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_MVP>/assets"
)
