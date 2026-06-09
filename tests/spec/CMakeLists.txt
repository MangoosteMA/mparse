add_executable(mparse_spec_tests
    generated_parser_test.cpp
    spec_reader_test.cpp
)

target_compile_features(mparse_spec_tests PRIVATE cxx_std_23)

target_compile_definitions(mparse_spec_tests PRIVATE
    kMparseProjectSourceDir="${PROJECT_SOURCE_DIR}"
    kMparseTestSourcesDir="${CMAKE_CURRENT_SOURCE_DIR}/sources"
)

target_link_libraries(mparse_spec_tests PRIVATE
    mparse_spec
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mparse_spec_tests)

set(MPARSE_GENERATED_SPEC_PARSER_CHECK_OUTPUT
    "${CMAKE_CURRENT_BINARY_DIR}/generated_parser.cpp"
)

add_test(
    NAME regenerate_spec_parser_snapshot
    COMMAND
        $<TARGET_FILE:mparse>
        --grammar ${PROJECT_SOURCE_DIR}/src/spec/spec.grammar
        --output ${MPARSE_GENERATED_SPEC_PARSER_CHECK_OUTPUT}
        --root-symbol Spec
)

add_test(
    NAME generated_spec_parser_is_up_to_date
    COMMAND
        ${CMAKE_COMMAND}
        -E
        compare_files
        ${PROJECT_SOURCE_DIR}/src/spec/generated/generated_parser.cpp
        ${MPARSE_GENERATED_SPEC_PARSER_CHECK_OUTPUT}
)

set_tests_properties(
    generated_spec_parser_is_up_to_date
    PROPERTIES DEPENDS regenerate_spec_parser_snapshot
)
