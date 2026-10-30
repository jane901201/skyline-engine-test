cmake_minimum_required(VERSION 3.25)

project(DayX_Light LANGUAGES CXX)

add_executable(DayX_Light WIN32
        main.cpp
)

target_compile_features(DayX_Light PRIVATE cxx_std_20)

target_link_libraries(DayX_Light PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_Light)

set_target_properties(DayX_Light PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Light>"
)

add_custom_command(TARGET DayX_Light POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Light>/assets"
)
