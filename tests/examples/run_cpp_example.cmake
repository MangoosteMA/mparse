foreach(required_variable
        MPARSE_EXECUTABLE
        CXX_COMPILER
        EXAMPLE_SOURCE
        WORKING_DIR)
    if(NOT DEFINED ${required_variable})
        message(FATAL_ERROR "${required_variable} is required")
    endif()
endforeach()

file(MAKE_DIRECTORY "${WORKING_DIR}")

set(generated_source "${WORKING_DIR}/hex_color.generated.cpp")
set(generated_binary "${WORKING_DIR}/hex_color_example")
if(WIN32)
    set(generated_binary "${generated_binary}.exe")
endif()

execute_process(
    COMMAND "${MPARSE_EXECUTABLE}"
        --grammar "${EXAMPLE_SOURCE}"
        --output "${generated_source}"
    RESULT_VARIABLE generation_result
    OUTPUT_VARIABLE generation_output
    ERROR_VARIABLE generation_error
)

if(NOT generation_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to generate hex color example.\n"
        "stdout:\n${generation_output}\n"
        "stderr:\n${generation_error}"
    )
endif()

execute_process(
    COMMAND "${CXX_COMPILER}"
        -std=c++23
        "${generated_source}"
        -o "${generated_binary}"
    RESULT_VARIABLE compile_result
    OUTPUT_VARIABLE compile_output
    ERROR_VARIABLE compile_error
)

if(NOT compile_result EQUAL 0)
    message(FATAL_ERROR
        "Failed to compile generated hex color example.\n"
        "stdout:\n${compile_output}\n"
        "stderr:\n${compile_error}"
    )
endif()

execute_process(
    COMMAND "${generated_binary}"
    RESULT_VARIABLE run_result
    OUTPUT_VARIABLE run_output
    ERROR_VARIABLE run_error
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

if(NOT run_result EQUAL 0)
    message(FATAL_ERROR
        "Generated hex color example failed.\n"
        "stdout:\n${run_output}\n"
        "stderr:\n${run_error}"
    )
endif()

if(NOT run_output STREQUAL "26 240 156")
    message(FATAL_ERROR
        "Unexpected hex color example output: '${run_output}'"
    )
endif()
