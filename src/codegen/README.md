# Code Generation

This directory contains the language-independent codegen entry point and the
current C++ backend.

The codegen layer does not analyze grammars by itself. It receives:

- `spec::Specification`: parsed grammar symbols, rules, actions, and source
  template.
- `analysis::SymbolsStorage`: resolved symbol metadata.
- `analysis::GrammarAutomata`: automata built by the analysis layer.
- `GenerationOptions`: target language and optional root symbol name.

`codegen::generate` in `codegen.cpp` is the public dispatcher. Today it only
supports `Language::Cpp` and forwards the request to `cpp::generateCpp`.

## C++ Backend Flow

The C++ backend is structured as a small pipeline:

1. `cpp_codegen.cpp` chooses the root symbol.
   If `GenerationOptions::root_symbol_name` is empty, the first symbol in the
   specification is used.
2. `render_model.cpp` converts the specification and automata into a JSON
   render model. Most codegen decisions happen here.
3. `templates.cpp` exposes embedded Inja templates from
   `cpp/templates/*.inja.inc`.
4. `renderer.cpp` renders a template with the JSON model.
5. The caller inserts the rendered parser into the original source template.
   This happens outside this directory, in `src/main.cpp`.

The final generated file has these sections:

1. Generated-code banner.
2. Commented copy of the source grammar.
3. Runtime support classes.
4. Forward declarations for parser and vertex generators.
5. Semantic action functions.
6. Parser and vertex generator classes.
7. Public `parse(...)` function for the selected root symbol.

## Runtime Model

Generated parsers are lazy generators. They can explore ambiguous grammars
without materializing every parse result at once.

- `mparse_generated_detail::PartialGenerator`: abstract generator that yields
  `PartialResult`.
- `PartialResult`: current input position plus a semantic stack stored as
  `std::vector<std::any>`.
- `SequentialPartialGenerator`: helper for generators that try one child
  generator at a time and continue after the current child is exhausted.
- `SinglePartialGenerator`: emits one already-computed `PartialResult`.

For every grammar symbol, codegen emits one parse generator class and one
vertex generator class per automaton vertex.

The parse generator owns the start vertex generator. Its `next()` method skips
partial results with an empty stack and casts the top stack item to the symbol
value type.

Each vertex generator stores:

- `input_`: original input text.
- `position_`: current input offset.
- `stack_`: semantic values accumulated so far.
- symbol parameters, if the grammar symbol has arguments.
- `terminal_pending_`: whether the vertex should first emit its terminal
  result.
- `edge_index_`: next outgoing edge to try.

An edge generator either rejects the edge by returning `nullptr`, or returns a
new partial generator for the edge target.

## Transitions

The C++ backend currently handles five transition kinds:

- `LiteralTransition`: checks that the input at `position_` starts with the
  literal, pushes the matched string, and advances by the literal length.
- `RangeTransition`: checks one input character against an inclusive range,
  pushes the matched character, and advances by one byte.
- `RepeatedLiteralTransition`: checks a literal repeated by a raw C++ count
  expression, pushes the matched string, and advances by the repeated byte
  length.
- `SymbolTransition`: creates a nested parse generator for another grammar
  symbol. Every nested result appends its semantic value to the current stack
  and continues at the nested result position.
- `ReduceTransition`: calls the generated action function for the current
  stack, replaces the stack with the returned semantic value, and moves to the
  target vertex without consuming input.

## Actions

Semantic action code is emitted by `cpp/action_emitter.cpp`.

The analysis layer gives codegen action trees. This matters for nullable or
expanded symbols: a single grammar action can depend on semantic values that are
produced by nested synthetic actions. Codegen first assigns stable numeric IDs
to action tree nodes, then emits one `mparse_action_<id>` function per node.

Action text is rewritten before emission:

- `$$` becomes the local result variable `ret`.
- `$1`, `$2`, ... become typed reads from the semantic stack through
  `mparse_generated_detail::semanticValue<T>(args, index)`.

If an action node has children, the action emitter first builds `resolved_args`
by executing child action functions over their input slices. The user action is
then rewritten against that resolved argument list.

If a generated action is a pure semantic-value forwarding node, it returns the
single forwarded argument instead of creating a typed `ret`.

## Identifier And Literal Escaping

`cpp/identifiers.cpp` owns generated C++ names and literal escaping.

- Symbol names are converted to valid C++ identifiers.
- Parse generators are named `mparse_parse_<symbol>_generator`.
- Vertex generators are named
  `mparse_parse_<symbol>_vertex_<index>_generator`.
- Actions are named `mparse_action_<index>`.
- String and character literals are escaped before insertion into templates.

Keep escaping centralized here. Templates should receive already-safe C++
fragments whenever possible.

## Templates

Templates are stored as raw string fragments under `cpp/templates` and included
from `templates.cpp`. `render_model.cpp` renders small templates directly while
building larger generated sections.

- `cpp_file.cpp.inja.inc`: top-level file layout. Receives the pre-rendered
  sections listed above.
- `runtime.cpp.inja.inc`: generated runtime support: result structs, generator
  base classes, and `semanticValue`.
- `action_function.cpp.inja.inc`: one static `std::any` function for a semantic
  action tree node.
- `parse_generator_declaration.cpp.inja.inc`: declaration of one public parse
  generator class for a grammar symbol.
- `parse_generator_definition.cpp.inja.inc`: constructor and `next()`
  implementation for a parse generator.
- `vertex_generator_declaration.cpp.inja.inc`: declaration of one vertex
  generator class for one automaton vertex.
- `vertex_generator_constructor.cpp.inja.inc`: vertex generator constructor,
  including copied symbol parameters and initial terminal state.
- `vertex_generator_make_next.cpp.inja.inc`: lazy driver for terminal result
  emission and outgoing edge enumeration.
- `vertex_generator_make_edge.cpp.inja.inc`: `switch` over outgoing edge
  indexes.
- `literal_edge_generator_case.cpp.inja.inc`: edge case for literal
  transitions.
- `range_edge_generator_case.cpp.inja.inc`: edge case for range transitions.
- `symbol_edge_generator_case.cpp.inja.inc`: edge case for nested symbol
  parsing.
- `reduce_edge_generator_case.cpp.inja.inc`: edge case for semantic reduce
  transitions.
- `root_parse_function.cpp.inja.inc`: public
  `parse(std::string_view, ...)` function. It returns the first root parse
  result that consumes the whole input.

## Changing Codegen

For small formatting changes, edit the relevant template and update canon files
if generated output intentionally changes:

```bash
cmake --build build --target update_canons
ctest --test-dir build --output-on-failure
```

For behavior changes, start in `render_model.cpp`. That file decides which
classes, edge cases, action functions, and template variables are emitted.

When adding a new transition kind, update:

1. Analysis data structures and automata construction.
2. `emitEdgeGeneratorCase` in `render_model.cpp`.
3. A dedicated edge-case template under `cpp/templates`, if the code is not
   trivial.
4. Codegen tests and canon files under `tests/codegen`.

When changing action semantics, update `action_emitter.cpp` and add tests that
cover nested action trees, forwarding actions, and typed `$n` access.
