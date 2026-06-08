include(FetchContent)

# CLI11 is a lightweight header-only library for command-line parsing.
# It generates --help output, validates required options, and reports parse
# errors without us maintaining custom argc/argv parsing code.
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG v2.5.0
)

FetchContent_MakeAvailable(CLI11)

if(MPARSE_BUILD_TESTS)
    # GoogleTest is the unit-test framework used by the test targets under
    # tests/. Tests link to gtest_main, so individual test files only define
    # TEST/TEST_F cases and do not need their own main().
    FetchContent_Declare(
        googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.15.2
    )

    FetchContent_MakeAvailable(googletest)
endif()
