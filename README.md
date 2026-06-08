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
