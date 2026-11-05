cmake_minimum_required(VERSION 3.25)

project(DayX_BlockSelect LANGUAGES CXX)

add_executable(DayX_BlockSelect WIN32
        main.cpp
)

target_compile_features(DayX_BlockSelect PRIVATE cxx_std_20)

target_link_libraries(DayX_BlockSelect PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_BlockSelect)

set_target_properties(DayX_BlockSelect PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

add_custom_command(TARGET DayX_BlockSelect POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_BlockSelect>/assets"
)
