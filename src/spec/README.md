# Specification Reader

This directory owns the grammar file format used by mparse.

The spec layer is responsible for:

- reading a file from disk;
- extracting the block between `// mparse: begin grammar` and
  `// mparse: end grammar`;
- preserving the surrounding source template;
- parsing grammar symbols, rules, rule items, actions, and symbol arguments;
- reporting parse errors with source line and column information.

It does not resolve symbol references, check grammar cycles, or build parser
automata. Those steps belong to `src/analysis`.

## Data Model

`data.h` contains the public AST-like representation passed to later stages.

- `Specification`: parsed `symbols` plus the original `source_template`.
- `SourceTemplate`: text before and after the grammar block. Codegen output is
  inserted between these parts.
- `Symbol`: grammar symbol name, source location, optional value type, optional
  parameters, and rules.
- `Rule`: sequence of rule items plus an optional action body.
- `RuleItem`: one of literal, range, or symbol reference.
- `SymbolArgument`: parameter declaration from `Symbol[name Type, ...]`.

Literals carry `std::string` values, ranges carry single `char` boundaries, and
symbol references carry a target name plus raw argument expressions.

## Read Path

The production read path starts in `spec.cpp`:

1. `readSpecificationOrThrow(path)` reads the whole file through `readFile`.
2. `extractGrammarBlock` finds the begin/end markers and returns:
   - extracted grammar text;
   - first grammar line number in the original file;
   - `SourceTemplate` for reinserting generated code.
3. `Parser` parses the grammar text and receives the first line offset so
   diagnostics and `Symbol::source_reference` point to the original file.
4. The resulting `Specification` is returned to analysis and codegen.

`readSpecification(path)` is the CLI-facing wrapper. It catches exceptions,
prints the error, and exits through `mparse::exitWithError`.

## Current Syntax

The current spec syntax intentionally stays small:

```text
SymbolName[argument_name ArgumentType]<ValueType>
    : RuleItem RuleItem { $$ = $1 }
    : OtherRule
```

Supported rule items:

- `'literal'`: literal text. Escapes currently include `\n`, `\t`, `\\`, and
  `\'`; unknown escapes are kept as the escaped character.
- `'a'-'z'`: inclusive one-character range.
- `OtherSymbol`: symbol reference without arguments.
- `OtherSymbol[arg1, call(arg2)]`: symbol reference with raw C++ argument
  expressions.

Actions are parsed as balanced `{ ... }` blocks. Brackets inside character
literals are ignored while balancing, so actions and argument lists can contain
nested calls, templates, and initializer lists.

Grouped expressions and quantifiers are explicitly rejected for now.

## Manual Parser

`parser.cpp` is a direct recursive-descent parser for the current syntax.

Important details:

- It tracks `position_`, `line_`, and `column_` while consuming characters.
- `parseBalanced(open, close)` handles nested brackets and quoted character
  literals.
- `parseCommaSeparatedExpressions` splits only at top-level commas.
- `parseRule` consumes rule items until newline or action start.
- `parseLiteralOrRange` decides whether a literal is standalone or the left
  boundary of a range.

This parser is still the production implementation used by `spec.cpp`.

## Self-Hosted Grammar Snapshot

`spec.grammar` is the canonical self-hosted grammar for this same spec syntax.
It is also a source template: helper C++ code appears outside the grammar block,
and generated parser code is inserted at the marker position.

The generated snapshot lives under:

- `generated/generated_parser.h`
- `generated/generated_parser.cpp`

The snapshot is committed because it breaks the bootstrap cycle: a clean
checkout can build `mparse` without requiring another already-installed
`mparse` binary.

To update the snapshot after editing `spec.grammar`:

```bash
cmake --build build --target regenerate_spec_parser
```

The test suite contains a CTest check that regenerates the snapshot into the
build directory and compares it with the committed file.

The generated parser currently returns parsed symbols for the extracted grammar
text. The surrounding source-template extraction still lives in `utils.cpp`.

## Tests

Spec tests live under `tests/spec`.

They cover:

- regular spec parsing;
- literal and range edge cases;
- escaped literal characters;
- source template reinsertion;
- unsupported grouped expressions;
- generated parser parity with the manual parser;
- snapshot freshness for `spec.grammar`.

Run them through the full test suite:

```bash
ctest --test-dir build --output-on-failure
```

## Changing The Spec Format

When adding syntax:

1. Update `data.h` if the new construct needs representation after parsing.
2. Update the manual parser while it remains the production parser.
3. Update `spec.grammar` with the same behavior.
4. Regenerate `generated/generated_parser.cpp`.
5. Add fixtures or parity tests under `tests/spec`.
6. Check whether analysis and codegen need to understand the new construct.

Keep `spec.grammar` as the long-term source of truth. The manual parser should
only stay as long as it is still needed for a safe migration.
