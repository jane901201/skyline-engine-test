# 主控台範例：顯示 Unity 風格的 Awake / Start / Update 生命週期與執行順序，不需要視窗或 GPU
add_executable(DayX_Gameplay main.cpp)
target_compile_features(DayX_Gameplay PRIVATE cxx_std_20)
target_link_libraries(DayX_Gameplay PRIVATE MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(DayX_Gameplay)
