# 指定此專案要求的最低 CMake 版本
cmake_minimum_required(VERSION 3.25)

# 建立名為 DayX_Triangle 的專案
project(DayX_Triangle LANGUAGES CXX)

# 建立一個名為 DayX_Triangle 的可執行檔 Target。
# main.cpp 為此 Target 需要編譯的原始碼。
add_executable(DayX_Triangle WIN32
        main.cpp
)

# 指定此專案至少需要 C++20。
target_compile_features(DayX_Triangle PRIVATE
        cxx_std_20
)

# 設定 DayX_Triangle 需要連結的 Library。
target_link_libraries(DayX_Triangle PRIVATE #PRIVATE 表示這個相依性只屬於 DayX_Triangle
        # MyDXEngineSDK::MyDXEngineSDK：是透過 add_library(... ALIAS ...) 來找到
        MyDXEngineSDK::MyDXEngineSDK 
)

# 複製 DayX_Triangle 執行時需要的相依檔案 EX:DLL
copy_runtime_dependencies(DayX_Triangle)

# 設定 Visual Studio 啟動 Debug 時的 Working Directory
set_target_properties(DayX_Triangle PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
)

# add_custom_command 會在編譯完成之後執行額外指令
add_custom_command(TARGET DayX_Triangle POST_BUILD

        COMMAND ${CMAKE_COMMAND} -E copy_directory

        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"

        "$<TARGET_FILE_DIR:DayX_Triangle>/assets"
)