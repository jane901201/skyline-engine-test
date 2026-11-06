add_executable(SkylineAudioTests audio_device_test.cpp)
target_compile_features(SkylineAudioTests PRIVATE cxx_std_20)
target_link_libraries(SkylineAudioTests PRIVATE Catch2::Catch2WithMain MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(SkylineAudioTests)
# 所有測試都使用 headless（Null 後端），不需要實體音訊裝置，可在 CI 執行
catch_discover_tests(SkylineAudioTests PROPERTIES LABELS "audio")
