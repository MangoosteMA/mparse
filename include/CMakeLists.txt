add_library(mparse_common
    mparse/strongly_connected_components.cpp
    mparse/utils.cpp
)

target_compile_features(mparse_common PUBLIC cxx_std_23)

target_include_directories(mparse_common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)
