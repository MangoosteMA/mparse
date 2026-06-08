add_subdirectory(analysis)
add_subdirectory(codegen)
add_subdirectory(spec)

add_executable(mparse
    main.cpp
    cli/options.cpp
)

target_compile_features(mparse PRIVATE cxx_std_23)

target_include_directories(mparse PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(mparse PRIVATE
    mparse_codegen
    CLI11::CLI11
)
