# Project Context
moku is a Gomoku engine written in C++17. It supports hotseat PvP in one terminal, an engine mode following the Gomoku AI Protocol, and playing against a Minimax agent with Alpha-Beta pruning. Python bindings (`pymoku`, via pybind11) expose core logic for reinforcement learning.

## Structure
- `include/` – headers (`.hpp`), mirrored subfolders (e.g. `include/minimax/`)
- `src/` – implementations (`.cpp`), mirrored subfolders (e.g. `src/minimax/`)
- `tests/` – GoogleTest unit tests
- `qlearning/` – Simple implementation of reinforcement learning with QLearning
- `py_bindings` – Python bindings
- Build system: CMake (>= 3.10)

## Board Representation
- Cells stored as `std::bitset<MAX_SQUARES>` for `black` and `white` on `Board`.
- Index mapping: `pos = row * g_config.board_size + col`.
- Board size is runtime-configurable via `g_config.board_size` (5–19).

## Running the Executable
Invocation: `moku [options]`.

### Game Modes
- `--engine`, `-e` – engine mode (Gomoku AI Protocol over stdin/stdout).
- `--pvp`, `-p` – hotseat Player vs Player in one terminal.
- `--minimax`, `-m` – Player vs Minimax agent (default if no mode given).

### Settings
- `--size`, `-s <n>` – board size, integer 5–19 (default 15).
- `--depth`, `-d <n>` – max search depth for the Minimax agent (default 12).
- `--time`, `-t <ms>` – search time limit in ms (default 20000).
- `--cores`, `-c <n>` – CPU cores to use (default: max available; clamped to hardware concurrency).
- `--debug` – enable debug output on stderr.

### Helpers
- `--version`, `-v` – print version and exit.
- `--help`, `-h` – print usage and exit.
- `--benchmark`, `-b` – run benchmark tests (optionally specify depth and time limit). `--benchmark 10 5000` runs benchmarks with depth 10 and a 5-second time limit.

Example: `moku --pvp --size 13 --time 10000`.

## Engine Mode / Gomoku Protocol
In engine mode moku implements the [Gomocup Protocol](https://plastovicka.github.io/protocl2en.htm). Commands are read line-by-line from stdin; responses go to stdout and are flushed after every response. Command keywords are case-insensitive. Coordinates use the `X,Y` format where `pos = Y * board_size + X`.

Supported commands:
- `START [size]` – initialize board of given size (default 15, valid 5–20). Responds `OK` or `ERROR ...`.
- `BEGIN` – engine plays the first move (as black). Responds with `X,Y`.
- `TURN X,Y` – opponent's move; engine updates state and replies with its own `X,Y`.
- `BOARD ... DONE` – full board state; each line `X,Y,field` (`field` 1 = own, 2 = opponent) until `DONE`, then engine replies `X,Y`.
- `INFO key value` – set parameters (`timeout_turn`, `timeout_match`, `time_left`, `max_memory`, `game_type`, `rule`, `folder`, `depth`); no response.
- `RESTART` – reset board. Responds `OK`.
- `TAKEBACK X,Y` – undo the stone at position. Responds `OK` or `ERROR ...`.
- `RECTSTART` – not supported; responds `ERROR ...`.
- `END` – terminate the engine loop.

Debug-only commands (not part of the protocol):
- `DISPLAY` / `D` – print board and stone counts.
- `HELP` – print available commands as `MESSAGE ...` lines.

Unknown commands yield `UNKNOWN <cmd>`.

## Naming Conventions
- Functions and variables: `snake_case`.
- Classes and structs: `PascalCase`.
- Constants and macros: `UPPER_SNAKE_CASE`.
- No `using namespace std;`, especially not in headers.

## Comments
- Do NOT use inline comments inside function bodies.
- Describe all behavior, parameters, and return values in the function docstring above the definition.
- Use the existing docstring format:
  - A short description of what the function does.
  - `@param name ; type ; description` for each parameter.
  - `@return type ; description`.
- File-level docstrings at the top of a file are allowed to describe the file's purpose.

## Performance
- `evaluate_board`, `analyze_line_fast`, `score_window`, and `is_line_dead` are hot paths.
- No heap allocations in hot paths.
- Prefer `inline` for small hot-path helpers.
- Maintain const-correctness.

## Build & Test
- Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Build everything: `cmake --build build --config Release`
- Build executable: `cmake --build build --config Release --target moku`
- Build tests: `cmake --build build --config Release --target run_tests`
- Build Python lib: `cmake --build build --config Release --target pymoku`
- Compiler warnings are strict (`/W4` on MSVC, `-Wall -Wextra -pedantic` otherwise); generated code must compile warning-free.

## Do / Don't
- Make small, surgical changes; do not rewrite whole files for minor edits.
- Keep new headers guarded (`#pragma once`).
- When adding new source files, register them in `CMakeLists.txt` for every relevant target.

## Response Style
- Be concise. No filler, no preambles.
- Always return an explanation with the code.