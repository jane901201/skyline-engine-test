add_executable(SkylineSceneTests
        transform_test.cpp
        camera_test.cpp
        scene_test.cpp
        fly_camera_test.cpp
        euler_test.cpp
        model_importer_test.cpp
)
target_compile_features(SkylineSceneTests PRIVATE cxx_std_20)
target_link_libraries(SkylineSceneTests PRIVATE Catch2::Catch2WithMain MyDXEngineSDK::MyDXEngineSDK)
# 匯入測試會讀取範例資產資料夾裡的 FBX
target_compile_definitions(SkylineSceneTests PRIVATE
        SKYLINE_ASSET_DIR="${CMAKE_SOURCE_DIR}/iTHelpContest30/assets")
copy_runtime_dependencies(SkylineSceneTests)
# 場景、Transform、Camera 都只依賴數學與輸入狀態，不需要 GPU 或視窗
catch_discover_tests(SkylineSceneTests PROPERTIES LABELS "scene")
