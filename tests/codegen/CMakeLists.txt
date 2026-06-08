add_executable(mparse_codegen_tests
    codegen_test.cpp
)

target_compile_features(mparse_codegen_tests PRIVATE cxx_std_23)

target_link_libraries(mparse_codegen_tests PRIVATE
    mparse_codegen
    mparse_test_utils
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mparse_codegen_tests)
