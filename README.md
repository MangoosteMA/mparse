# mparse

## Build

Configure the project:

```bash
cmake -S . -B build
```

Build all targets:

```bash
cmake --build build
```

Tests are enabled by default. To configure without tests:

```bash
cmake -S . -B build -DMPARSE_BUILD_TESTS=OFF
```

## Tests

Run the full test suite:

```bash
ctest --test-dir build --output-on-failure
```

## Canon Files

Some tests compare text output with canon files stored next to their test
suite, for example under `tests/analysis/canons`.
After intentionally changing user-facing output, update the canon files with:

```bash
cmake --build build --target update_canons
```

Then run the tests again:

```bash
ctest --test-dir build --output-on-failure
```
