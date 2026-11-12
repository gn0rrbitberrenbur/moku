# Test Module Reference
moku uses GoogleTest for core and protocol checks. CMake builds three test
executables and registers their individual cases with CTest through
`gtest_discover_tests()`.

## 1. Test Targets and Coverage
| Target | Source | Main coverage |
| --- | --- | --- |
| `moku_tests` | `tests/tests.cpp` | Conversion, board, evaluation, Gomocup engine, transposition table, and search metrics |
| `gomocup_protocol_tests` | `tests/gomocup_protocol_test.cpp` | Gomocup command responses, required board-size support, BOARD records, INFO silence, ABOUT formatting, and input line handling |
| `extended_protocol_tests` | `tests/extended_protocol_test.cpp` | Extended INFO node and TT memory limits, search paths, metrics, and help |

Core suite names include `AlgebraicConversion`, `BoardTest`, `EvaluatorTest`,
`EngineTest`, `TranspositionTableTest`, and `SearchMetricsTest`. The dedicated protocol fixture
is `GomocupProtocolTest`.

Checks include row-major conversion, move mutation, winning lines, window
scores, nearby candidates, command settings, hash consistency, replacement
policy, mate-score normalization, TT occupancy under concurrent stores,
per-search counter resets, completed depths, live worker gauges, and final
debug snapshots for fixed-depth, serial timed, and SMP search.
Memory-budget checks cover rounding, minimum capacity, shrinking and restoring
capacity, cache invalidation, stable Zobrist keys, small-table collision locking,
and extended-protocol `max_memory` behavior.

## 2. Configuration and Build
From the repository root, in PowerShell:

```powershell
cmake -S . -B build-engine -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=OFF
cmake --build build-engine --config Release --target moku_tests gomocup_protocol_tests extended_protocol_tests
ctest --test-dir build-engine -C Release --output-on-failure --no-tests=error
```

Standalone CMake configuration fetches GoogleTest v1.14.0, so initial
configuration may need network access. Use a consistent build directory;
replace the example path if the existing configured build is elsewhere.

## 3. Focused Runs
List discovered cases without running them:

```powershell
ctest --test-dir build-engine -C Release -N
```

Select related suites with one CTest regular expression:

```powershell
ctest --test-dir build-engine -C Release --output-on-failure -R "^(BoardTest|EvaluatorTest)\."
ctest --test-dir build-engine -C Release --output-on-failure -R "^(SearchMetricsTest|TranspositionTableTest)\."
ctest --test-dir build-engine -C Release --output-on-failure -R "^GomocupProtocolTest\."
```

For a multi-configuration Windows build, a direct GoogleTest run can use:

```powershell
& .\build-engine\engine\moku\Release\moku_tests.exe --gtest_filter="TranspositionTableTest.*"
```

Executable locations differ for single-configuration generators. CTest avoids
hard-coding those binary locations.

## 4. Protocol Test Harness
Test subclasses expose protected command dispatch and board state.
The dedicated `ProtocolEngine` redirects stdout to a string stream and, when
needed, stdin to a supplied multiline input. It invokes the handler directly
rather than starting a child process.

The fixture resets selected global settings before each case. Its coordinate
helper checks the `X,Y` response shape, line ending, and coordinate bounds.
Protocol checks include the required 20x20 board size, legal replies after
`TURN` and `BOARD`, field 3 handling, no stdout response to `INFO`, machine-readable
`ABOUT` output, ignored empty input lines, and CRLF command input. Optional
commands are tested only where their behavior is defined by the reference.
These tests dispatch through the engine in-process; they are not a complete
child-process pipe or tournament-compliance test.

## 5. Adding and Interpreting Tests
- Add cases to the appropriate existing source and suite.
- Configure global dimensions before constructing agents or tables.
- Save and restore shared settings and redirected streams when a test changes them.
- Use board size 20 for specification conformance and small search budgets for
  focused checks.
- Check actual state and response content, not just whether a call returned.

## 6. Implementation Locations
- [tests.cpp](../moku/tests/tests.cpp): core and engine suites.
- [gomocup_protocol_test.cpp](../moku/tests/gomocup_protocol_test.cpp): protocol fixture and cases.
- [CMakeLists.txt](../moku/CMakeLists.txt): GoogleTest setup and CTest discovery.
- [Build workflow](../.github/workflows/build.yml): current CI scope.