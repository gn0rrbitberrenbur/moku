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
- `engine/moku/tests/engine_tests/` contains core module and search-metrics tests;
  `engine/moku/tests/protocol_tests/` contains stream-based protocol dispatch tests.
- `engine/docs/docsOverview.md` links the module references. Read the corresponding
  game, tools, evaluation, search, TT, protocol, configuration, or benchmark reference.

## Configuration And Board Contracts

- Defaults: size 15, maximum depth 12, turn limit 20000 ms, debug output false,
  metrics interval 10 ms, unlimited nodes (`max_nodes=0`), and hardware thread
  count with a fallback of one.
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
  `get_completed_depth()` exposes the completed-depth counter for consumers such as
  dataset quality gating; zero means no root iteration completed.
  Time checks are cooperative. Its root candidates carry move, black-positive score,
  and node count; each root candidate is searched with an independent full window
  that reuses only exact transposition-table entries at the same depth.
- Fixed-depth search updates its root score. Timed single-thread search also refreshes
  the candidate collection. Multiworker SMP publishes the deepest completed result
  from any worker, while root score/candidate metadata retain their prior values.
  Check each entry point before consuming statistics.
- SMP uses worker-local board copies, shared TT/counters/cancellation, and joined
  helper threads. TT probe/store operations use striped mutexes; metrics use atomic
  counters/gauges. Keep global configuration and positional-cache dimensions stable.
  Use `g_config.cores=1` for serial checks.
- `g_config.max_nodes=0` disables the node limit. A positive limit counts recursive
  minimax nodes (not root candidates) and is shared across SMP workers. Node claims
  must use the shared atomic counter so concurrent searches never exceed the limit.
  An interrupted search returns its best available legal move.
- Each agent owns a direct-mapped table with a default and maximum of `2^23` slots.
  An explicit TT byte budget rounds down to the largest fitting power-of-two capacity.
  Zero restores the default; positive budgets must fit at least one `TTEntry`.
  `size()` reports capacity. Resize only without concurrent search; changing capacity
  clears entries and occupancy, preserves Zobrist keys, and releases old storage.
  Lock striping uses slot indices so even tables smaller than 256 slots are safe.
  Zobrist generation uses seed 12345 and construction-time geometry. Incremental hash
  updates toggle a stone and side-to-move. Preserve mate-score ply normalization and
  exact/lower/upper bound handling when editing probe/store.
- Empty TT slots use depth `-1`; hash-zero probes only hit occupied entries.
  Occupancy counts filled slots once, including collision replacement, and resets
  on `clear()` with no concurrent search. Allocated bytes measure entry-vector storage.
- Debug search events use `[METRICS]` JSON on stderr. Preserve `search_progress` and
  the interval-independent `search_finished` event on every search entry point,
  including early returns. Include `tt_capacity_entries`, `tt_allocated_bytes`,
  `tt_occupancy_permille`, `tt_probes_total`, `tt_hits_total`, `depth_completed`,
  `workers_active`, `alpha_beta_cutoffs_total`, and `root_candidates_count`.
  Search counters reset per call and aggregate SMP workers; TT occupancy persists.
  Completed depth only advances after a full root iteration; final active workers
  are zero. Consult `engine/docs/docsSearch.md` for metric semantics.
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
  `max_memory`, `game_type`, `rule`, `folder`, `depth`, and, in the extended
  protocol, `max_nodes`. Depth accepts 1 through 20; `max_nodes` accepts nonnegative
  values, with zero disabling the limit. Several fields are stored metadata; trace
  their consumers before assigning behavior.
- Only the extended handler enforces `INFO max_memory <bytes>` as a TT entry-vector
  budget and passes it to subsequent benchmark agents. Parse exactly one nonnegative
  `long long`; invalid input or allocation failure retains the table and setting and
  writes an error to stderr, with no stdout response. `START`/`RESTART` retain capacity.
  Standard Gomocup handling remains metadata-only. The budget is per table and excludes
  other engine storage, allocator overhead, and transient resize peak memory.
- Protocol search budget is `max(100, timeout_turn - 500)` for a positive turn timeout,
  otherwise 5000 ms. `INFO` writes diagnostics, when applicable, to stderr.
- The extended dispatcher adds `DISPLAY`/`D`, `HELP`, `BENCHMARK depth time_limit_ms`,
  and `METRICS [interval_ms]`. Metrics toggling accepts at most one positive integer
  interval and replies exactly `METRICS True` or `METRICS False`.
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
cmake --build build-engine --config Release --target gomocup_moku extended_moku moku_tests gomocup_protocol_tests extended_protocol_tests
ctest --test-dir build-engine -C Release --output-on-failure --no-tests=error
```

The core library contains board, coordinate conversion, evaluation, search, and TT
sources. Native protocol and test targets link this library through their engine
libraries. Register core additions in its source list. Python and NNUE consume
`moku::core`; changes to these APIs also require their focused checks.

Executables are under `build-engine/engine/moku/`, with `Release/` for
multi-configuration generators. GoogleTest discovers suite/case names. Use `ctest -N`
to choose a narrow filter; run `extended_protocol_tests` for extended dispatch changes.
Concurrency changes require checks beyond serial protocol tests.

The WebAssembly target is `extended_moku_wasm`. It requires an activated Emscripten
SDK and a separate CMake tree:

```sh
emcmake cmake -S . -B build/wasm -DCMAKE_BUILD_TYPE=Release -DMOKU_BUILD_WASM=ON -DMOKU_BUILD_ENGINE=OFF -DMOKU_BUILD_TESTS=OFF -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=OFF
cmake --build build/wasm --config Release --target extended_moku_wasm
```

Use `scripts/build_wasm.ps1` or `scripts/build_wasm.sh` for the corresponding
platform. The WASM outputs are under `build/wasm/engine/moku/`. The full build scripts
also run this dedicated build after the native build and therefore require Emscripten.

Follow root and `engine/.clang-format` formatting. Update affected module references,
README content, and these instructions with current implementation facts in English.