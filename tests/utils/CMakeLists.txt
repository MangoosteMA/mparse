add_library(mparse_test_utils
    canon.cpp
    spec_builders.cpp
)

target_compile_features(mparse_test_utils PUBLIC cxx_std_23)

target_compile_definitions(mparse_test_utils PRIVATE
    kMparseTestCanonsDir="${CMAKE_CURRENT_SOURCE_DIR}/.."
)

target_include_directories(mparse_test_utils PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mparse_test_utils PUBLIC
    mparse_spec
)
