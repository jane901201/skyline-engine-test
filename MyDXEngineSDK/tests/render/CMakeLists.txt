add_executable(SkylineShaderTests triangle_shader_test.cpp pipeline_integration_test.cpp device_factory_test.cpp compute_readback_test.cpp resource_factory_test.cpp command_context_test.cpp)

target_compile_features(SkylineShaderTests PRIVATE cxx_std_20)
target_link_libraries(SkylineShaderTests PRIVATE
    Catch2::Catch2WithMain
    MyDXEngineSDK::MyDXEngineSDK
)
target_compile_definitions(SkylineShaderTests PRIVATE
    "TRIANGLE_SHADER_PATH=\"${CMAKE_SOURCE_DIR}/iTHelpContest30/assets/shaders/triangle.hlsl\""
    "RHI_TEST_SHADER_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/pipeline_test.hlsl\""
    "RHI_COMPUTE_SHADER_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/compute_readback.hlsl\""
    "RHI_TEXTURE_SHADER_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/texture_test.hlsl\""
    "RHI_MESH_PATH=\"${CMAKE_CURRENT_SOURCE_DIR}/mesh_test.ply\""
    "RHI_EXAMPLE_SHADER_DIR=\"${CMAKE_SOURCE_DIR}/iTHelpContest30/assets/shaders/\""
)

copy_runtime_dependencies(SkylineShaderTests)
catch_discover_tests(SkylineShaderTests
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:SkylineShaderTests>"
    PROPERTIES LABELS "gpu")
