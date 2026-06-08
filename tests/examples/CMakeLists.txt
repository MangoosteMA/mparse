add_test(
    NAME Examples.HexColor
    COMMAND ${CMAKE_COMMAND}
        "-DMPARSE_EXECUTABLE=$<TARGET_FILE:mparse>"
        "-DCXX_COMPILER=${CMAKE_CXX_COMPILER}"
        "-DEXAMPLE_SOURCE=${PROJECT_SOURCE_DIR}/examples/cpp/hex_color.cpp"
        "-DWORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}/hex_color"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/run_cpp_example.cmake"
)

set_tests_properties(Examples.HexColor PROPERTIES
    LABELS examples
)
