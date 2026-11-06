# 主控台範例：顯示物理模擬與射線查詢的結果，不需要視窗或 GPU
add_executable(DayX_Physics main.cpp)
target_compile_features(DayX_Physics PRIVATE cxx_std_20)
target_link_libraries(DayX_Physics PRIVATE MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(DayX_Physics)
