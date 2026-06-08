add_subdirectory(cpp)

add_library(mparse_codegen
    codegen.cpp
)

target_compile_features(mparse_codegen PUBLIC cxx_std_23)

target_include_directories(mparse_codegen PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mparse_codegen PUBLIC
    mparse_analysis
    mparse_codegen_cpp
    mparse_spec
)
