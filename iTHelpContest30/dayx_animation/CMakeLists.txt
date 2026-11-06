# 主控台範例：顯示關鍵影格動畫、Animator 播放控制與骨架蒙皮矩陣的結果，不需要視窗或 GPU
add_executable(DayX_Animation main.cpp)
target_compile_features(DayX_Animation PRIVATE cxx_std_20)
target_link_libraries(DayX_Animation PRIVATE MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(DayX_Animation)
