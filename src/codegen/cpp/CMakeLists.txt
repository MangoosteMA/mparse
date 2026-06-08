add_library(mparse_codegen_cpp
    action_emitter.cpp
    cpp_codegen.cpp
    identifiers.cpp
    writer.cpp
)

target_compile_features(mparse_codegen_cpp PUBLIC cxx_std_23)

target_include_directories(mparse_codegen_cpp PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(mparse_codegen_cpp PUBLIC
    mparse_analysis
    mparse_spec
)
