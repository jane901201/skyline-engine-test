add_executable(SkylineAnimationTests
        animation_clip_test.cpp
        animator_test.cpp
        skeleton_test.cpp
)
target_compile_features(SkylineAnimationTests PRIVATE cxx_std_20)
target_link_libraries(SkylineAnimationTests PRIVATE Catch2::Catch2WithMain MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(SkylineAnimationTests)
# 動畫只依賴數學與場景，不需要 GPU 或視窗
catch_discover_tests(SkylineAnimationTests PROPERTIES LABELS "animation")
