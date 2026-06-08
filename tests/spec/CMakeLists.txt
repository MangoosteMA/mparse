add_executable(mparse_spec_tests
    spec_reader_test.cpp
)

target_compile_features(mparse_spec_tests PRIVATE cxx_std_23)

target_compile_definitions(mparse_spec_tests PRIVATE
    kMparseTestSourcesDir="${CMAKE_CURRENT_SOURCE_DIR}/sources"
)

target_link_libraries(mparse_spec_tests PRIVATE
    mparse_spec
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mparse_spec_tests)
