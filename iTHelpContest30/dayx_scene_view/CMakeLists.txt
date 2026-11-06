# 視窗範例：在真正的 3D 視窗裡顯示 Scene、Camera、物理、動畫與 GameWorld 行為
add_executable(DayX_SceneView WIN32 main.cpp)
target_compile_features(DayX_SceneView PRIVATE cxx_std_20)
target_link_libraries(DayX_SceneView PRIVATE MyDXEngineSDK::MyDXEngineSDK)

copy_runtime_dependencies(DayX_SceneView)

set_target_properties(DayX_SceneView PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DayX_SceneView>"
)

# 著色器放在 assets/shaders，執行時以相對路徑載入，所以要複製到執行檔旁邊
add_custom_command(TARGET DayX_SceneView POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../assets"
        "$<TARGET_FILE_DIR:DayX_SceneView>/assets"
)
