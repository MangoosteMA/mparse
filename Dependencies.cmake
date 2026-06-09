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

# Inja renders generated parser source from structured codegen data. It is
# header-only and uses nlohmann/json as its data model.
FetchContent_Declare(
    nlohmann_json
    URL https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz
)

FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    inja
    GIT_REPOSITORY https://github.com/pantor/inja.git
    GIT_TAG v3.5.0
    SOURCE_SUBDIR cmake/no-cmakelists
)

FetchContent_MakeAvailable(inja)

if(NOT TARGET pantor::inja)
    add_library(pantor_inja INTERFACE)
    target_include_directories(pantor_inja INTERFACE
        ${inja_SOURCE_DIR}/include
        ${inja_SOURCE_DIR}/single_include
    )
    target_link_libraries(pantor_inja INTERFACE
        nlohmann_json::nlohmann_json
    )
    add_library(pantor::inja ALIAS pantor_inja)
endif()

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
