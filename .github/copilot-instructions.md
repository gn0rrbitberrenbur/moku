# Project Context

moku is a Gomoku engine written in C++17. It supports hotseat PvP in one terminal, an engine mode following the Gomoku AI Protocol, and playing against a Minimax agent with Alpha-Beta pruning.

## Repository Structure

The repository contains the CMake project in the `moku/` directory:

- `moku/CMakeLists.txt` - CMake build configuration
- `moku/config.hpp` - global configuration and board settings
- `moku/main.cpp` - application entry point
- `moku/game/` - board logic
- `moku/game/tools/` - game-related conversion and utility functions
- `moku/engine/` - Gomoku protocol engine
- `moku/eval/` - board evaluation
- `moku/search/` - Minimax and Alpha-Beta search
- `moku/tt/` - transposition table
- `moku/benchmark/` - benchmark implementation
- `moku/tests/` - GoogleTest unit tests
- `moku/build/` - generated CMake build files and binaries
- `.github/workflows/build.yml` - GitHub Actions build and release workflow

All source and header files are located inside `moku/`. Include paths are relative to the `moku/` project directory.

## Build System

The project uses CMake 3.10 or newer and requires C++17.

Work from inside the project directory:

```bash
cd moku
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

The CMake targets are:

- `moku` - main executable
- `tests` - GoogleTest executable
- `moku_core` - static core library

Build only the main executable:

```bash
cmake --build moku/build --config Release --target moku
```

Build only the tests:

```bash
cmake --build moku/build --config Release --target tests
```

Run the test executable directly on Linux:

```bash
./moku/build/tests
```

On Windows:

```powershell
.\moku\build\Release\tests.exe
```

Run the main executable directly on Linux:

```bash
./moku/build/moku
```

On Windows:

```powershell
.\moku\build\Release\moku.exe
```

The project uses GoogleTest through CMake `FetchContent`.

## Running the Executable

Invocation on Linux:

```bash
./moku/build/moku [options]
```

Invocation on Windows:

```powershell
.\moku\build\Release\moku.exe [options]
```

### Game Modes

- `--engine`, `-e` - engine mode using the Gomoku AI Protocol
- `--pvp`, `-p` - hotseat Player vs Player in one terminal
- `--minimax`, `-m` - Player vs Minimax agent
- Minimax mode is used by default if no mode is specified

### Settings

- `--size`, `-s <n>` - board size, integer 5-19, default 15
- `--depth`, `-d <n>` - maximum Minimax search depth, default 12
- `--time`, `-t <ms>` - search time limit in milliseconds, default 20000
- `--cores`, `-c <n>` - number of CPU cores to use, clamped to hardware concurrency
- `--debug` - enable debug output on stderr

### Helpers

- `--version`, `-v` - print version and exit
- `--help`, `-h` - print usage and exit
- `--benchmark`, `-b` - run benchmark tests

Benchmark example:

```bash
./moku/build/moku --benchmark
```

Benchmark with depth and time limit:

```bash
./moku/build/moku --benchmark 10 5000
```

Example game:

```bash
./moku/build/moku --pvp --size 13 --time 10000
```

## Board Representation

The board stores black and white cells using `std::bitset<MAX_SQUARES>`.

The position index is calculated as:

```text
pos = row * g_config.board_size + col
```

The board size is runtime-configurable through `g_config.board_size`.

Valid board sizes are 5 through 19.

Coordinates use zero-based `X,Y` coordinates in engine protocol commands:

```text
pos = Y * board_size + X
```

## Engine Mode and Gomoku Protocol

Engine mode implements the Gomocup Protocol.

Commands are read line by line from stdin. Responses are written to stdout and flushed after every response. Command keywords are case-insensitive.

Supported commands:

- `START [size]` - initialize a board, default size 15
- `BEGIN` - engine plays the first move as black
- `TURN X,Y` - register the opponent's move and return the engine move
- `BOARD ... DONE` - load a complete board position and return the engine move
- `INFO key value` - set engine parameters
- `RESTART` - reset the board
- `TAKEBACK X,Y` - undo a stone
- `RECTSTART` - unsupported; returns an error
- `END` - terminate the engine loop

`INFO` parameters include:

- `timeout_turn`
- `timeout_match`
- `time_left`
- `max_memory`
- `game_type`
- `rule`
- `folder`
- `depth`

Debug-only commands:

- `DISPLAY`
- `D`
- `HELP`

Unknown commands return:

```text
UNKNOWN <cmd>
```

## Code Formatting

The project uses `.clang-format` in the repository root.

- Base style: LLVM
- Indentation: 4 spaces, never tabs
- Column limit: 100 characters
- Braces: Allman style
- Pointer and reference alignment: left
- Control statements use a space before parentheses
- Short functions may remain on one line only when empty
- Short `if` statements must not be placed on one line
- Includes are grouped and sorted case-sensitively
- Keep at most one consecutive empty line
- Preserve and reflow comments according to clang-format

## Performance

The following functions are performance-sensitive:

- `evaluate_board`
- `analyze_line_fast`
- `score_window`
- `is_line_dead`

For hot paths:

- Avoid heap allocations where possible
- Prefer `inline` for small helper functions
- Maintain const-correctness
- Avoid unnecessary board copies
- Preserve the existing bitset-based board representation

## GitHub Actions

The workflow in `build.yml` builds the project from the `moku` directory.

The workflow uses:

```yaml
working-directory: moku
```

for CMake configuration and building.

Linux artifact:

```text
moku/build/moku
```

Windows artifact:

```text
moku/build/Release/moku.exe
```

## Do and Don't

- Make small, surgical changes
- Keep changes consistent with the existing folder structure
- Register new source files in `CMakeLists.txt`
- Keep new headers guarded with `#pragma once`
- Do not rewrite whole files for minor changes
- Do not modify generated files under `build`
- Do not add unrelated refactors
- Ensure new source files are added to every relevant CMake target

## Response Style
- Be concise. No filler, no preambles.
- Always return an explanation with the code.