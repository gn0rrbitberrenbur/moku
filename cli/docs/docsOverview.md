# CLI module overview
## Purpose
Provide a standalone terminal Gomoku manager without changing the engine or its protocol output.
Application sources and CMake configuration live under `moku-cli/`. Automated test programs
live in the repository-level `tests/`, documentation in `docs/`, and build scripts in `scripts/`.

## Source layout
| Directory | Contents |
|---|---|
| `moku-cli/` | `main.cpp` and `CMakeLists.txt`; the CMake source root |
| `moku-cli/board/` | Board model and coordinate parsing |
| `moku-cli/engine/` | Protocol sessions, workers, engine errors and debug metrics/transcripts |
| `moku-cli/process/` | Native subprocess transport |
| `moku-cli/settings/` | Engine registry and game settings |
| `moku-cli/file_handling/` | Shared atomic file replacement and JSON parsing/escaping |
| `moku-cli/ui/` | Application, terminal and UI helpers |
| `moku-cli/tests/` | Runtime test mode, match runner and report generation |
| `tests/` | Automated test executables and mock engine |
| `moku-cli/build/` | Generated build artifacts, not application sources |

Project includes are relative to the inner `moku-cli/` source root, for example
`"board/board.hpp"` and `"tests/test_runner.hpp"`. CMake exports only this source root as
the project include directory. CMake source paths are relative to `moku-cli/CMakeLists.txt`;
the automated test sources therefore use `../tests/`.

## Modules
| Module | Responsibility | Reference |
|---|---|---|
| Configuration | Engine registry and per-game parameters | [docsConfig.md](docsConfig.md) |
| Board | Coordinates, legal placement, wins and draws | [docsBoard.md](docsBoard.md) |
| Engine | Protocol session, worker and subprocess transport | [docsEngine.md](docsEngine.md) |
| Terminal | Native keyboard input, rendering and application screens | [docsTerminal.md](docsTerminal.md) |
| Test mode | Engine series, statistics, Elo and JSON reports | [docsTestMode.md](docsTestMode.md) |
| Build and tests | Standalone CMake targets and verification | [docsBuild.md](docsBuild.md) |

## Data flow
`main -> EngineStore + Terminal -> Application -> Board + EngineWorker -> Process -> engine`

The UI owns the game board and validates every move. Workers own protocol sessions and subprocesses.
Futures carry startup/move results and exceptions back to the UI without blocking keyboard input.
An engine process does not share application memory and may use its own search threads.

Debug games: `Process observer -> DebugSession` writes a shared, serialized transcript for both
workers. Each worker parses its own metrics lines and publishes a latest snapshot;
`Application -> DebugSession` reads snapshots for in-place display.

Test mode: `TestMode -> runTest -> playTestGame -> EngineWorker` (two per game), with
`writeTestReport -> writeFileAtomically` after every game.