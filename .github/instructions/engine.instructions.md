---
description: "Use when editing the Gomoku board, evaluation, search, transposition table, protocols, benchmarks, engine tests, or engine documentation."
applyTo: "engine/**"
---
# Engine Instructions

## Ownership And Build Routing

Apply `.github/copilot-instructions.md` as the repository-wide baseline. Paths in
the ownership list below are relative to the repository root.

- Root `CMakeLists.txt` adds `engine/moku/`. Its `CMakeLists.txt` owns the active
  `moku_core` library, alias `moku::core`, protocol executables, and engine tests.
- `engine/CMakeLists.txt` is a separate build description. Trace the root
  `add_subdirectory` and actual source lists when choosing which target to modify.
- `engine/moku/config.hpp` defines mutable inline `g_config`/`GameConfig`.
- `engine/moku/game/board.*` owns board storage, geometry, mutation, and win checks;
  `engine/moku/game/tools/algebraic_to_index.*` owns text coordinate conversion.
- `engine/moku/eval/evaluation.*` owns candidates, pattern scores, and tactical ordering.
- `engine/moku/search/minimax.*` owns fixed-depth, timed, and Lazy SMP search/statistics.
- `engine/moku/tt/transposition_table.*` owns Zobrist keys, bounds, and replacement.
- `engine/moku/engine/gomocup_protocol/` and `engine/moku/engine/extended_protocol/`
  each define an `Engine` for a separate executable. Keep translation-unit sets separate.
- `engine/moku/benchmark/benchmark.*` owns workloads and printed benchmark results.
- `engine/moku/tests/tests.cpp` covers core modules and standard engine behavior;
  `engine/moku/tests/gomocup_protocol_test.cpp` covers stream-based Gomocup dispatch.
- `engine/docs/docsOverview.md` links the module references. Read the corresponding
  game, tools, evaluation, search, TT, protocol, configuration, or benchmark reference.

## Configuration And Board Contracts

- Defaults: size 15, maximum depth 12, turn limit 20000 ms, debug output false,
  metrics interval 10 ms, and hardware thread count with a fallback of one.
  Runtime version comes from `MOKU_VERSION`, falling back to `"unknown"`.
- Supported dimensions are 5 through 19. `MAX_SIZE=19`, `MAX_SQUARES=361`;
  `Board` stores black and white stones in separate fixed-capacity bitsets.
- Indexing is `row * board_size + column`; protocol coordinates are `column,row`.
  Board geometry reads the global configuration. Keep dimensions stable while
  positions, agents, and tables are in use; reconstruct geometry-dependent objects
  after a size change.
- Callers maintain in-range indices, legal placements, and disjoint color bitsets.
  `make_move` sets the selected color; `undo_move` clears both colors at that index.
  Wins use freestyle rules: a contiguous run of at least five stones wins.
- Algebraic coordinates use case-insensitive columns and one-based row numbers.
  The helper returns `-1` for rejected input; a nonpositive size selects global size.

## Evaluation, Search, And Hashing

- `Evaluator::evaluate_board` is black-positive; `evaluate_board_stm` adjusts sign
  for the supplied side to move. Preserve this distinction in callers and datasets.
- Evaluation combines center proximity, five-cell windows, and open tactical patterns.
  Candidates are sorted empty indices within Chebyshev distance two of existing stones;
  an empty board yields the center. Candidate moves differ from all empty squares.
- Alpha-beta search maximizes black and minimizes white. Search returns an index or
  `-1`; the caller commits the selected move. Preserve make/undo state consistency.
- Timed single-thread search uses iterative deepening and retains completed iterations.
  Time checks are cooperative. Its root candidates carry move, black-positive score,
  and node count; pruned search scores can represent bounds.
- Fixed-depth search updates its root score. Timed single-thread search also refreshes
  the candidate collection. Multiworker SMP publishes the deepest completed result
  from any worker, while root score/candidate metadata retain their prior values.
  Check each entry point before consuming statistics.
- SMP uses worker-local board copies, shared TT/counters/cancellation, and joined
  helper threads. Treat TT slots, shared depth, and the evaluator's mutable positional
  cache as synchronization-sensitive state. Use `g_config.cores=1` for serial checks.
- Each agent owns a direct-mapped table with `2^23` slots. `size()` reports capacity.
  Zobrist generation uses seed 12345 and construction-time geometry. Incremental hash
  updates toggle a stone and side-to-move. Preserve mate-score ply normalization and
  exact/lower/upper bound handling when editing probe/store.
- Keep search/evaluation hot paths allocation-conscious and avoid unnecessary board
  copies. Validate tactical decisions and state restoration after search changes.

## Protocol Contracts

Both executables read stdin lines synchronously, uppercase command keywords, and flush
responses on stdout. Search occupies the command loop until it returns. Preserve exact
protocol responses; emit diagnostics and metrics to stderr.

- Shared commands: `START [size]`, `BEGIN`, `TURN X,Y`, `BOARD` followed by records
  and `DONE`, `INFO key value`, `ABOUT`, `RESTART`, `TAKEBACK X,Y`, `END`, `RECTSTART`.
- `START` defaults to 15 and accepts sizes 5 through 19. `BEGIN` plays black, selecting
  the center on an empty board. `TURN` records the opponent move and replies using
  timed SMP; infer colors from existing stone counts.
- `BOARD` fields 1/2 denote own/opponent stones. The handler maps them to black/white
  using counts and replies through single-thread timed search. Preserve both-color
  position-loading and reply-state tests when editing this logic.
- Recognized case-sensitive `INFO` keys: `timeout_turn`, `timeout_match`, `time_left`,
  `max_memory`, `game_type`, `rule`, `folder`, `depth`. Depth accepts 1 through 20.
  Several fields are stored metadata; trace their consumers before assigning behavior.
- Protocol search budget is `max(100, timeout_turn - 500)` for a positive turn timeout,
  otherwise 5000 ms. `INFO` writes diagnostics, when applicable, to stderr.
- The extended dispatcher adds `DISPLAY`/`D`, `HELP`, `BENCHMARK depth time_limit_ms`,
  and `DEBUG_OUTPUT [interval_ms]`. Debug toggling accepts at most one positive integer
  interval and replies exactly `DEBUG_OUTPUT True` or `DEBUG_OUTPUT False`.
- Positive benchmark time selects SMP; zero selects fixed-depth search. Benchmarking
  disables debug output and restores board size on normal return. Report fields for
  configured depth and placeholder score/TT size require interpretation against
  `BenchResult` assignments.
- Unknown commands return `UNKNOWN <original-token>`. Rectangular starts return the
  protocol's unsupported-board error. Preserve error text and stdout/stderr separation.

## Build And Focused Checks

The root project requires CMake >=3.15 and C++17. The verification commands below use
CMake/CTest >=3.20 for `--test-dir` support. Run them from the repository root:

```sh
cmake -S . -B build-engine -DCMAKE_BUILD_TYPE=Release -DMOKU_BUILD_ENGINE=ON -DMOKU_BUILD_TESTS=ON -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=OFF
cmake --build build-engine --config Release --target gomocup_moku extended_moku tests gomocup_protocol_tests
ctest --test-dir build-engine -C Release --output-on-failure --no-tests=error
```

The core library contains board, coordinate conversion, evaluation, search, and TT
sources. The active engine executables and test targets also compile their own source
lists directly. Register additions in every affected list. Python and NNUE consume
`moku::core`; changes to these APIs also require their focused checks.

Executables are under `build-engine/engine/moku/`, with `Release/` for
multi-configuration generators. GoogleTest discovers suite/case names. Use `ctest -N`
to choose a narrow filter; use a stream-level extended-protocol smoke check for changes
to extended dispatch. Concurrency changes require checks beyond serial protocol tests.

Follow root and `engine/.clang-format` formatting. Update affected module references,
README content, and these instructions with current implementation facts in English.