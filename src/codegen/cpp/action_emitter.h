#pragma once

#include <analysis/automaton.h>
#include <codegen/cpp/writer.h>

#include <cstddef>

namespace mparse::codegen::cpp {

    void emitActionFunction(
        Writer& writer,
        size_t action_index,
        const analysis::ReduceTransition& reduce
    );

} // namespace mparse::codegen::cpp
