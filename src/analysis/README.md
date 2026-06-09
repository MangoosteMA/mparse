# Grammar Analysis

This directory turns parsed specification data into a form that codegen can
emit directly.

The analysis layer is responsible for:

- converting `spec::Specification` into resolved `Symbol` and `Rule` objects;
- validating symbol references and symbol argument counts;
- building action trees for semantic actions;
- expanding nullable rule items into explicit helper symbols;
- rejecting grammar shapes that can produce non-progressing parser loops;
- building per-symbol automata consumed by codegen;
- printing optional grammar statistics for the CLI.

It does not parse source files and does not emit target-language code.

## Public Entry Points

`analyze.h` exposes the main API:

- `analyzeOrThrow(specification, options)` returns `SymbolsStorage` or throws
  `AnalysisError`.
- `analyze(specification, options)` is the CLI-facing wrapper that prints and
  exits on error.
- `AnalysisOptions::check_nonprogressing_cycles` controls nullable-cycle
  validation. The CLI can disable it when the user wants to inspect generated
  output for an otherwise unsafe grammar.

The normal pipeline in `analyze.cpp` is:

1. `makeSymbolsStorageFromSpecification`.
2. `validateSymbolReferences`.
3. `expandEmptyItems`.
4. `resolveEmptyCycles`.
5. `findEmptyEndCycle`.
6. Return the normalized `SymbolsStorage`.

`SymbolsStorage::buildAutomata()` is then called by `src/main.cpp` after
analysis succeeds.

## Core Model

- `Symbol`: wrapper around `spec::Symbol`. It keeps both the current symbol and
  the original symbol so expanded helper symbols can still report diagnostics
  against user-written source locations.
- `Rule`: sequence of `spec::RuleItem` values plus an `ActionTreeNode`.
- `ActionTreeNode`: semantic action graph used after nullable expansion. A node
  can represent a user rule action, an empty-symbol placeholder, or forwarding
  of a semantic value. Edges describe which slice of the rule stack is replaced
  by a nested action result.
- `SymbolsStorage`: name-indexed container of analyzed symbols. It owns
  generated helper symbols, supports stable name lookup, and builds
  `GrammarAutomata`.

## Symbol Reference Validation

`validate_symbol_references.cpp` runs before nullable expansion, while rules
still match user-written symbols directly.

For every `RuleItemSymbol`, it checks:

- the referenced symbol exists;
- the number of call arguments matches the target symbol declaration.

The pass intentionally does not type-check argument expressions. Arguments are
raw target-language expressions, so C++ type checking happens when the generated
parser is compiled.

When validation fails, analysis reports either
`AnalysisErrorKind::UnknownSymbolReference` or
`AnalysisErrorKind::InvalidSymbolArgumentCount`.

## Nullable Expansion

The generated parser runtime cannot safely handle arbitrary nullable recursion
as plain epsilon transitions. `expandEmptyItems.cpp` rewrites grammars so
nullable symbols become explicit alternatives.

The algorithm starts with `findEmptySymbols`, which computes which symbols can
derive an empty value.

For every nullable symbol `S`, expansion may create helper symbols:

- `SBeforeEmptyPart`: rules that produce values before the empty alternative.
- `SAfterEmptyPart`: rules that produce values after the empty alternative.
- `S` itself keeps forwarding rules to those helper symbols, the empty rule,
  and supported self-starting rules.

When a nullable symbol appears inside another rule, the rule is expanded into
alternatives:

- keep the part before the nullable symbol;
- collapse the nullable symbol into its empty action;
- keep the part after the nullable symbol.

The action tree tracks these transformations so user actions still see `$1`,
`$2`, ... as if the original rule had been parsed directly.

Currently unsupported case:

- A self-starting rule that begins with two references to the same symbol, for
  example `S : S S`. This is rejected as
  `AnalysisErrorKind::UnsupportedSelfStartingRule`.

## Empty Cycle Checks

After nullable expansion, analysis rejects grammars that can loop without
consuming input.

- `resolve_empty_cycles.cpp`: builds a graph from each symbol to the first
  symbol reference in its rules. Strongly connected components are resolved by
  inlining eligible first-symbol rules. If a cycle remains, analysis reports
  `AnalysisErrorKind::NonProgressingCycle`.
- `find_empty_end_cycles.cpp`: finds self-starting automaton paths whose tail
  can be empty. These would let a parser reach the same end state without
  consuming input, so analysis reports `AnalysisErrorKind::EmptyEndCycle`.

Both errors preserve original source references even when the cycle goes
through expanded helper symbols.

## Automata

`automaton.cpp` builds one `SymbolAutomaton` per analyzed symbol.

Every automaton has:

- `start`: vertex `0`;
- `end`: vertex `1`;
- one path per rule;
- one transition per rule item;
- one reduce transition at the end of each rule path.

Transition types:

- `LiteralTransition`: consumes a string literal and produces `std::string`.
- `RangeTransition`: consumes one character in an inclusive range and produces
  `char`.
- `SymbolTransition`: runs another symbol parser and produces that symbol's
  value type.
- `ReduceTransition`: runs an action tree and produces the current symbol's
  value type.

Self-starting rules are special-cased: if a rule starts with an exact reference
to its own symbol, the path begins at the automaton end vertex and skips that
first item. This lets codegen model left-recursive growth from an already
parsed value.

Common prefixes are intentionally not merged yet. The code keeps separate rule
paths until determinism and priority semantics are explicit enough to preserve
user-visible behavior.

## Statistics

`statistics.cpp` collects and prints grammar statistics for the CLI verbose
mode.

The default mode aggregates expanded helper symbols back into original symbols.
Higher verbosity can print per-symbol details and expanded symbols directly.

Canon files for this output live under `tests/analysis/canons/statistics`.

## Tests

Analysis tests live under `tests/analysis`.

They cover:

- automaton construction;
- nullable expansion;
- non-progressing cycle detection and resolution;
- empty end-cycle detection;
- grammar statistics output.

When intentionally changing formatted diagnostics or statistics output, update
canon files with:

```bash
cmake --build build --target update_canons
ctest --test-dir build --output-on-failure
```

For algorithm changes, add focused tests near the affected behavior before
updating canon output.

## Changing Analysis

Use this checklist for non-trivial analysis changes:

1. Add or update spec fixtures/builders for the grammar shape being changed.
2. Update the relevant algorithm under `algo/`.
3. Preserve original source references on cloned or generated symbols.
4. Verify action trees still describe the same user-facing `$n` values.
5. Check generated automata with `tests/analysis/automaton_test.cpp`.
6. Run codegen tests, because codegen consumes `GrammarAutomata` and action
   trees directly.

Analysis is the compatibility boundary between parsing and code generation. If
the shape of `Symbol`, `Rule`, `ActionTreeNode`, or `AutomatonTransition`
changes, expect corresponding changes in `src/codegen`.
