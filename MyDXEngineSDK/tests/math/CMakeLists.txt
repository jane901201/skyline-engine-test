add_executable(SkylineMathTests
    vector2_test.cpp
    vector3_test.cpp
    vector4_test.cpp
    matrix_test.cpp
    ../../math/vector.cpp
)

target_compile_features(SkylineMathTests PRIVATE cxx_std_20)
target_compile_definitions(SkylineMathTests PRIVATE MYDXENGINESDK_STATIC)
target_include_directories(SkylineMathTests PRIVATE ../..)
target_link_libraries(SkylineMathTests PRIVATE
    Catch2::Catch2WithMain
    Microsoft::DirectXMath
)

catch_discover_tests(SkylineMathTests)
