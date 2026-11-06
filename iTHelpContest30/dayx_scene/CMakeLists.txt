# 主控台範例：顯示場景階層、攝影機、剔除與拾取的結果，不需要視窗或 GPU
add_executable(DayX_Scene main.cpp)
target_compile_features(DayX_Scene PRIVATE cxx_std_20)
target_link_libraries(DayX_Scene PRIVATE MyDXEngineSDK::MyDXEngineSDK)
copy_runtime_dependencies(DayX_Scene)
