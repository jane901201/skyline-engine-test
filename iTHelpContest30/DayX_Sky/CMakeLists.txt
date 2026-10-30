cmake_minimum_required(VERSION 3.25)

project(DayX_Sky LANGUAGES CXX)

add_executable(DayX_Sky WIN32
        main.cpp
)

target_compile_features(DayX_Sky PRIVATE cxx_std_20)

target_link_libraries(DayX_Sky PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_Sky)

set_target_properties(DayX_Sky PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Sky>"
)

add_custom_command(TARGET DayX_Sky POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Sky>/assets"
)
