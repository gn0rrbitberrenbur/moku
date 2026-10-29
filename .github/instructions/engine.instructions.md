---
applyTo: "engine/**"
---
# Project Context
moku is a C++17 Gomoku engine. It builds two protocol-driven executables: `gomocup_moku` for
the Gomocup protocol and `extended_moku`, which adds debugging and benchmark commands. The
search uses Minimax with alpha-beta pruning, iterative deepening, move ordering, a Zobrist-hashed
transposition table, and optional Lazy SMP.

## Repository Structure
The CMake project and all C++ sources are under `moku/`:

- `moku/CMakeLists.txt` - build configuration and target definitions
- `moku/config.hpp` - global board, search, time, and thread configuration
- `moku/main_gomocup.cpp` - Gomocup executable entry point
- `moku/main_extended_protocol.cpp` - extended executable entry point
- `moku/game/` - bitset board and game rules
- `moku/game/tools/` - coordinate conversion utilities
- `moku/engine/gomocup_protocol/` - Gomocup command handling
- `moku/engine/extended_protocol/` - Gomocup commands plus extended commands
- `moku/eval/` - position evaluation, candidate moves, and move ordering
- `moku/search/` - Minimax, alpha-beta, iterative deepening, and Lazy SMP
- `moku/tt/` - Zobrist hashing and transposition table
- `moku/benchmark/` - benchmark positions and reporting
- `moku/tests/tests.cpp` - core, evaluator, engine, and transposition-table tests
- `moku/tests/gomocup_protocol_test.cpp` - Gomocup protocol tests
- `.github/workflows/build.yml` - Linux/Windows build and tagged-release workflow
- `.clang-format` - C++ formatting rules

There is no interactive PvP application or command-line option parser. Both executables read
protocol commands from standard input and write protocol responses to standard output.

## Build and Test
The project requires CMake 3.10 or newer and C++17. GoogleTest is fetched through CMake
`FetchContent`, so configuring the standalone project may require network access.

From the repository root:

```bash
cmake -S moku -B moku/build -DCMAKE_BUILD_TYPE=Release
cmake --build moku/build --config Release
ctest --test-dir moku/build -C Release --output-on-failure
```

From the repository root, the convenience build scripts can be run as follows:

```powershell
.\scripts\build.ps1
```

```bash
bash scripts/build.sh
```

CMake targets:

- `gomocup_moku` - Gomocup protocol executable
- `extended_moku` - executable with extended debug and benchmark commands
- `moku_core` - static core library
- `tests` - core and engine GoogleTest executable
- `gomocup_protocol_tests` - Gomocup protocol GoogleTest executable

On Linux, the executables are `moku/build/gomocup_moku` and
`moku/build/extended_moku`. On Windows with a multi-configuration generator, they are
`moku/build/Release/gomocup_moku.exe` and `moku/build/Release/extended_moku.exe`.

To build one target, use `cmake --build moku/build --config Release --target <target>`.

## Configuration and Board
Defaults in `GameConfig` are board size 15, maximum search depth 12, and a 20,000 ms turn time
limit. The configured thread count defaults to hardware concurrency, with a fallback of one.
Neither executable provides command-line configuration options; the former `extended_moku`
`--debug-output` flag has been removed. Gomocup `INFO` commands update settings such as
`timeout_turn` and `depth` at runtime. Debug output defaults to disabled.

The board uses two `std::bitset<MAX_SQUARES>` values, with `MAX_SIZE` set to 19 and
`MAX_SQUARES` set to 361. Positions are row-major: `pos = row * board_size + col`. Gomocup
coordinates are zero-based `X,Y`, so `pos = Y * board_size + X`.

Important implementation discrepancy: both protocol handlers currently accept `START 20`, but
the board bitsets only have capacity for 19x19. Treat 5 through 19 as the supported range; do
not extend board-size claims without resolving this mismatch.

The CMake project version is `0.0.5`, while `GameConfig::version` (used by `ABOUT`) is `0.0.7`.
Do not assume these version values are synchronized.

## Gomocup Protocol
Commands are read line by line from stdin. Responses go to stdout and are flushed. Command
keywords are case-insensitive. The Gomocup executable supports:

- `START [size]` - initialize an empty board; defaults to 15
- `BEGIN` - play the first move, using the center on an empty board
- `TURN X,Y` - register the opponent move and return the engine move
- `BOARD ... DONE` - load a position and return a move; fields 1 and 2 represent own and
	opponent stones
- `INFO key value` - update supported game/search parameters
- `ABOUT` - return engine name and configured version
- `RESTART` - clear the board
- `TAKEBACK X,Y` - remove a stone
- `END` - stop the input loop
- `RECTSTART` - return an unsupported-board error

Recognized `INFO` keys are `timeout_turn`, `timeout_match`, `time_left`, `max_memory`,
`game_type`, `rule`, `folder`, and `depth` (depth values 1 through 20). Unknown commands return
`UNKNOWN <command>`. `INFO` does not produce a standard-output response.

`extended_moku` supports the same command handling and additionally recognizes `DISPLAY`/`D`,
`HELP`, and `BENCHMARK <depth> <time_limit_ms>`. These are not standard Gomocup commands.
There is no implemented `ANALYZE` command.

The extended engine defines `cmd_debug_output()` and lists `DEBUG_OUTPUT` in `HELP`.
The handler toggles `g_config.debug_output` and replies on stdout with exactly
`DEBUG_OUTPUT True` or `DEBUG_OUTPUT False`. However, `process_command()` does not yet
dispatch this command: stdin currently receives `UNKNOWN DEBUG_OUTPUT`. Do not claim
runtime toggling works until the dispatcher calls the handler. Conditional diagnostics
remain on stderr; `BENCHMARK` disables debug output without restoring its previous value.

## Implementation Notes
- The evaluator scores tactical patterns, gap-aware five-cell windows, and center proximity.
- Candidate moves are empty squares within a two-square neighborhood of existing stones; an
	empty board yields the center square.
- The search uses black-positive evaluation scores, alpha-beta pruning, and a transposition
	table. `TURN` uses the timed SMP search; search paths are performance-sensitive.
- Keep the bitset representation and avoid unnecessary allocations or board copies in hot paths.
- The protocol tests cover Gomocup handling, including TURN state consistency for both engine
  colors (legal reply, correct stone colors, preserved stones, and no overlap).
  There are no dedicated extended-protocol tests.

## Code Formatting
Follow `.clang-format` in the repository root:

- LLVM base style, 4-space indentation, no tabs, and a 100-character column limit
- Allman braces; left pointer and reference alignment
- Spaces before control-statement parentheses; no spaces inside parentheses
- Only empty short functions may remain on one line; short `if` statements stay multiline
- Regroup and case-sensitively sort includes
- Keep at most one consecutive blank line and preserve/reflow comments with clang-format

## CI and Releases
`.github/workflows/build.yml` runs on `ubuntu-latest` and `windows-latest` for `v*` tags or
manual dispatch. It builds `gomocup_moku` and `extended_moku` and uploads platform artifacts.
The workflow does not run the test targets.

## Change Guidelines
- Keep changes focused and consistent with the existing C++17 style and folder structure.
- For every change, review and update `.github/copilot-instructions.md` and the affected
  documentation in `docs/` to keep them consistent with the implementation. Update `README.md`
  when its content is affected; do not leave stale instructions or documentation.
- Write documentation in English. Maintain one reference per module using the
  `docsModuleName.md` naming convention, follow the structure of `docs/docsConfig.md`, and
  keep `docs/docsOverview.md` and its module links current when modules are added or changed.
- Add new source files to every applicable target in `moku/CMakeLists.txt`.
- Use `#pragma once` for new headers.
- Do not modify generated build output or make unrelated refactors.
- Preserve protocol output on stdout; send diagnostic output to stderr.
- Run the relevant GoogleTest/CTest checks after behavior changes when available.

## Response Style
Be concise and explain code changes without filler.