add_executable(SkylineGameplayTests
        game_world_test.cpp
)
target_compile_features(SkylineGameplayTests PRIVATE cxx_std_20)
target_link_libraries(SkylineGameplayTests PRIVATE Catch2::Catch2WithMain MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(SkylineGameplayTests)
# 行為生命週期只依賴場景與輸入狀態，不需要 GPU 或視窗
catch_discover_tests(SkylineGameplayTests PROPERTIES LABELS "gameplay")
