# Repository Overview
moku is a C++17 Gomoku engine. It provides a reusable core library and two
console executables controlled through line-based protocols.

This documentation describes the implementation as it stands, including known
limitations. Examples assume a supported board size and valid positions.

## 1. Documentation Index
| Module | Documentation | Responsibility |
| --- | --- | --- |
| Configuration | [docsConfig.md](docsConfig.md) | Global defaults and runtime settings |
| Game | [docsGame.md](docsGame.md) | Bitset board, coordinates, and win detection |
| Tools | [docsTools.md](docsTools.md) | Algebraic notation conversion |
| Evaluation | [docsEvaluation.md](docsEvaluation.md) | Position scoring, candidates, and move ordering |
| Search | [docsSearch.md](docsSearch.md) | Minimax, alpha-beta, iterative deepening, and Lazy SMP |
| Transposition table | [docsTranspositionTable.md](docsTranspositionTable.md) | Zobrist hashes and cached search results |
| Engine | [docsEngine.md](docsEngine.md) | Executable entry points and command processing |
| Gomocup protocol | [docsGomocupProtocol.md](docsGomocupProtocol.md) | Standard commands and compatibility notes |
| Extended protocol | [docsExtendedProtocol.md](docsExtendedProtocol.md) | Display, help, benchmark commands |
| Benchmark | [docsBenchmark.md](docsBenchmark.md) | Predefined search workloads and reporting |
| Tests | [docsTests.md](docsTests.md) | GoogleTest suites and focused validation |
| Build and release | [docsBuild.md](docsBuild.md) | CMake targets, dependencies, and CI |

## 2. Runtime Architecture
```text
stdin
  -> Engine command loop
       -> shared GameConfig
       -> Board
       -> MinimaxAgent
            -> Evaluator
            -> TranspositionTable
       -> stdout protocol responses
       -> stderr diagnostics
```

The extended executable adds access to board display and the benchmark runner.
Its debug-toggle handler is implemented but not yet registered for stdin commands;
see [DEBUG_OUTPUT](docsExtendedProtocol.md#5-debug_output). The former startup flag
has been removed.
The two protocol implementations are separate; they are not linked into the same
executable.

## 3. Shared Conventions
- `true` means black and `false` means white in board and search APIs.
- Positions are row-major integer indices: `row * board_size + column`.
- Protocol coordinates are zero-based `X,Y`, where `X` is the column.
- Algebraic coordinates use letters and one-based rows, for example `H8`.
- Evaluation and search scores are black-positive unless an API explicitly says
  otherwise. Black maximizes and white minimizes.
- Protocol replies belong on stdout; local diagnostic output belongs on stderr.
- Board dimensions come from the global configuration, not from each board.

## 4. Implementation Locations
- [Source tree](../moku): all C++ sources and the CMake project.
- [Project README](../README.md): quick build and usage instructions.
- [CMake configuration](../moku/CMakeLists.txt): library and executable targets.
- [Configuration header](../moku/config.hpp): shared settings.