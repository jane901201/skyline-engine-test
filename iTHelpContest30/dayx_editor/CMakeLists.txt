# 編輯器：用 ImGui 排出 Viewport、Outliner、Details、Content Browser 與 Output Log，
# 場景畫在 Viewport 面板的矩形裡，可以匯入 FBX 並放進場景
add_executable(DayX_Editor WIN32
        main.cpp
        asset_library.cpp
        asset_library.h
        editor_actions.cpp
        editor_actions.h
        editor_log.h
        editor_panels.h
        editor_state.h
        editor_ui.cpp
        viewport_panel.cpp
        outliner_panel.cpp
        details_panel.cpp
        content_browser_panel.cpp
)
target_compile_features(DayX_Editor PRIVATE cxx_std_20)
# comdlg32：匯入用的檔案對話框
target_link_libraries(DayX_Editor PRIVATE MyDXEngineSDK::MyDXEngineSDK comdlg32)

copy_runtime_dependencies(DayX_Editor)

set_target_properties(DayX_Editor PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_Editor>"
)

# 著色器與範例模型放在 assets，執行時以相對路徑載入，所以要複製到執行檔旁邊
add_custom_command(TARGET DayX_Editor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_Editor>/assets"
)
