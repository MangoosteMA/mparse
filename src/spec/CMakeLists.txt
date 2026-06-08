add_library(mparse_spec
    data.cpp
    parser.cpp
    spec.cpp
    utils.cpp
)

target_compile_features(mparse_spec PUBLIC cxx_std_23)

target_include_directories(mparse_spec PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mparse_spec PRIVATE
    mparse_common
)
