add_library(mparse_analysis
    algo/find_empty_symbols.cpp
    algo/expand_empty_items.cpp
    algo/resolve_empty_cycles.cpp
    analyze.cpp
    error.cpp
    rule.cpp
    statistics.cpp
    symbol.cpp
    symbols_storage.cpp
    utils/rules.cpp
    utils/symbol_rules.cpp
)

target_compile_features(mparse_analysis PUBLIC cxx_std_23)

target_include_directories(mparse_analysis PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_link_libraries(mparse_analysis PUBLIC
    mparse_common
)

target_link_libraries(mparse_analysis PRIVATE
    mparse_spec
)
