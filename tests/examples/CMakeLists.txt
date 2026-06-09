function(add_cpp_example_test name source expected_output)
    add_test(
        NAME ${name}
        COMMAND ${CMAKE_COMMAND}
            "-DMPARSE_EXECUTABLE=$<TARGET_FILE:mparse>"
            "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
            "-DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/${source}"
            "-DEXPECTED_OUTPUT=${expected_output}"
            "-DWORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/${name}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpp_example.cmake"
    )

    set_tests_properties(${name} PROPERTIES
        LABELS examples
    )
endfunction()

add_cpp_example_test(
    Examples.HexColor
    examples/cpp/hex_color.cpp
    "26 240 156"
)

add_cpp_example_test(
    Examples.NestedActions
    examples/cpp/nested_actions.cpp
    "ABc"
)

add_cpp_example_test(
    Examples.ParameterizedOutline
    examples/cpp/parameterized_outline.cpp
    "project > parser > grammar | parser > grammar"
)
