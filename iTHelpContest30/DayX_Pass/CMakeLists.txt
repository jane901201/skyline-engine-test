cmake_minimum_required(VERSION 3.25)

project(DayX_Pass LANGUAGES CXX)

add_executable(DayX_Pass WIN32
        main.cpp
)

target_compile_features(DayX_Pass PRIVATE cxx_std_20)

target_link_libraries(DayX_Pass PRIVATE
        MyDXEngineSDK::MyDXEngineSDK
)

copy_runtime_dependencies(DayX_Pass)

set_target_properties(DayX_Pass PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Pass>"
)

add_custom_command(TARGET DayX_Pass POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Pass>/assets"
)
