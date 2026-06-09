add_executable(mparse_analysis_tests
    automaton_test.cpp
    empty_cycle_test.cpp
    expand_empty_items_test.cpp
    statistics_test.cpp
    symbol_reference_validation_test.cpp
)

target_compile_features(mparse_analysis_tests PRIVATE cxx_std_23)

target_link_libraries(mparse_analysis_tests PRIVATE
    mparse_analysis
    mparse_test_utils
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(mparse_analysis_tests)

add_custom_target(update_analysis_canons
    COMMAND
        ${CMAKE_COMMAND} -E env
        MPARSE_UPDATE_CANONS=1
        $<TARGET_FILE:mparse_analysis_tests>
        --gtest_filter=GrammarStatistics.*:Analyze.ThrowsOnUnresolved*
    DEPENDS mparse_analysis_tests
    COMMENT "Update analysis canon files"
)

add_dependencies(update_canons update_analysis_canons)
