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

## Examples

The `examples/cpp/hex_color.cpp` file is a small self-contained C++ grammar
template. It parses an RGB color in `#RRGGBB` form and prints decoded channel
values:

```bash
build/src/mparse --grammar examples/cpp/hex_color.cpp --output /tmp/hex_color.generated.cpp
c++ -std=c++23 /tmp/hex_color.generated.cpp -o /tmp/hex_color
/tmp/hex_color
```

Expected output:

```text
26 240 156
```

The example is also checked by the CTest suite.

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
