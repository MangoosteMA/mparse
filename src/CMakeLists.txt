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

set(MPARSE_SPEC_GRAMMAR "${PROJECT_SOURCE_DIR}/src/spec/spec.grammar")
set(MPARSE_GENERATED_SPEC_PARSER
    "${PROJECT_SOURCE_DIR}/src/spec/generated/generated_parser.cpp"
)

add_custom_target(regenerate_spec_parser
    COMMAND
        $<TARGET_FILE:mparse>
        --grammar ${MPARSE_SPEC_GRAMMAR}
        --output ${MPARSE_GENERATED_SPEC_PARSER}
        --root-symbol Spec
    DEPENDS
        mparse
        ${MPARSE_SPEC_GRAMMAR}
    COMMENT "Regenerating the bootstrap spec parser"
    VERBATIM
)
