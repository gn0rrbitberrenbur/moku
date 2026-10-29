---
description: "Use when editing the pymoku pybind11 API, Python package, NumPy board conversion, extension build, or binding tests; also consult for Python packaging changes."
applyTo: "python_bindings/**,pyproject.toml"
---
# Python Bindings Instructions

## Ownership And Build Integration

Apply `.github/copilot-instructions.md`. Paths below are repository-relative.

- `python_bindings/CMakeLists.txt` creates `_core` from `src/bindings.cpp`, links
  local `moku::core` and pybind11, and installs the extension library under `pymoku`.
- `python_bindings/src/bindings.cpp` owns every Python-visible C++ registration,
  argument name/default, return policy, and Python-specific board helper.
- `python_bindings/python/pymoku/__init__.py` re-exports `._core` symbols.
  The adjacent `py.typed` is the package's typing marker.
- `python_bindings/tests/test_pymoku.py` is the pytest suite;
  `python_bindings/README.md` is the installation/API reference.
- Root `pyproject.toml` owns scikit-build-core metadata, CMake feature switches,
  wheel package paths, test extra, and cibuildwheel settings. `VERSION` supplies
  the dynamic package version.
- Read engine instructions and the owning core header/implementation for changes
  that alter engine semantics rather than Python adaptation.

## Toolchain And Packaging

Use CMake >=3.15 and C++17. Python metadata requires Python >=3.10, build dependencies
`scikit-build-core>=0.10` and `pybind11>=3.0`, runtime `numpy<2.5`, and optional
`test` dependency pytest. cibuildwheel selects CPython 3.10 through 3.14. CMake first
finds an installed pybind11 package, otherwise fetches v3.0.1.

Wheel CMake settings enable Python and disable engine executables, C++ tests, and
NNUE. Check metadata readme paths, `wheel.packages`, CMake install destinations, and
cibuildwheel test paths against the source tree above. Verify installed wheel contents
and import behavior when editing packaging. Inspect translation-unit ownership and
link definitions when exporting free functions, including benchmark functions.

## Public API And Semantics

Maintain existing Python names, named arguments, defaults, and return shapes unless
the task explicitly changes the public API. The package exports:

- `Board`, `RootCandidate`, `MinimaxAgent`, `Evaluator`, `TranspositionTable`,
  `GameConfig`, `BenchResult`.
- `get_config`, `set_board_size`, `get_board_size`, `algebraic_to_index`,
  `run_position`, `run_benchmark`.

### Boards And Global Configuration

- `Board()` exposes `make_move(pos,is_black)`, `undo_move(pos)`, `test_pos(pos)`,
  `check_win()`, `wins_at(pos,is_black)`, `output_board()`, geometry helpers
  `size`, `squares`, `pos`, `row`, `col`, and `legal_moves`/`to_numpy`.
- Indices are `row * board_size + column`. Callers maintain legal in-range placements,
  alternating turns, and disjoint colors. Undo clears either color at the index.
  `legal_moves()` lists every empty active square. Wins use at least five in a row.
- `to_numpy()` allocates an independent `numpy.int8` copy with shape `(2,B,B)`:
  black plane first, white plane second. This differs from NNUE's own/opponent planes.
- `set_board_size(n)` validates 5 through 19; `get_board_size()` returns global size.
  `get_config()` exposes the live process-global `GameConfig` by reference. Writable
  fields are `board_size`, `max_depth`, `time_limit_ms`, `debug_output`, `version`,
  and `cores`; `squares()` derives the area. Direct field assignment uses C++ field
  semantics, so prefer the validated board-size setter in examples and tests.
- Set geometry before constructing boards, agents, or tables. Keep it stable while
  those objects are in use; construct fresh objects after a dimension change.
- `algebraic_to_index(input,size=0)` accepts case-insensitive columns and one-based
  rows, uses global size when `size<=0`, and returns `-1` for rejected input.

### Search, Evaluation, And Hashing

- `MinimaxAgent(depth=6)` exposes fixed-depth, timed, and timed-SMP move selection,
  `set_max_depth`/`get_max_depth`, `get_last_root_score`, `get_last_root_candidates`,
  `get_nodes_searched`, `get_tt_hits`, and `clear_tt`. Search takes a board and
  `is_black`; timed calls also take `time_limit_ms`. Callers supply positive depth.
- Search returns an index or `-1`; callers play the returned move. The constructor's
  default depth is distinct from the global configuration default of 12.
- Root scores are black-positive. `RootCandidate` exposes read-only `move`, `score`,
  `nodes`. Timed single-thread search refreshes candidates from a completed iteration;
  use this path when consuming candidate metadata. Interpret statistics per entry point
  using the engine contract. The STL vector conversion returns a Python list; preserve
  deliberate ownership/return policies when editing this interface.
- `Evaluator()` exposes `evaluate_board`, `evaluate_board_stm`, `get_valid_moves`,
  `move_priority`, `position_score`. Main scores are black-positive, STM scores use
  the supplied side. Candidate moves use a radius-two neighborhood, or the center
  on an empty board, rather than the complete `Board.legal_moves()` set.
- `TranspositionTable()` exposes `compute_hash(board,is_black_turn)`,
  `update_hash(hash,pos,is_black)`, `clear()`, `size()`. Hash values are unsigned
  64-bit C++ integers; incremental updates also toggle side-to-move. `size()` is
  the fixed capacity `2^23`, rather than the number of populated entries.

### Benchmarks

- `run_position(name,setup,side_to_move,depth,time_ms,board_size=15)` accepts a
  sequence of `(algebraic_coordinate,is_black)` pairs and returns `BenchResult`.
- Read-only result fields: `name`, `reached_depth`, `nodes`, `tt_hits`, `tt_size`,
  `time_s`, `score`, `best_move`. Current assignments use configured maximum depth
  for `reached_depth` and zero for `score`/`tt_size`.
- Positive time selects SMP; zero selects fixed-depth search. Board size restores
  on normal return. `run_benchmark(depth,time_ms)` prints results, returns 0, and
  disables global debug output. Account for global effects in callers and tests.

## Lifetimes And Concurrency

Python owns directly constructed C++ objects; `get_config()` references static storage.
NumPy board copies own their storage. `RootCandidate`, `GameConfig`, and `BenchResult`
are obtained through their owning operations. Review STL conversion and lifetime
policies together before changing reference-returning APIs.

Binding calls retain the GIL. Internal SMP still launches C++ worker threads; global
configuration, evaluator caches, shared TT slots, and shared search depth require
explicit synchronization analysis. Use one engine core for deterministic serial
smoke checks. Any GIL-release change requires a lifetime/configuration/thread-safety
design and focused concurrency checks.

## Build And Focused Verification

From the repository root, a direct extension-only build is:

```sh
cmake -S . -B build-python -DCMAKE_BUILD_TYPE=Release -DMOKU_BUILD_ENGINE=OFF -DMOKU_BUILD_TESTS=OFF -DMOKU_BUILD_PYTHON=ON -DMOKU_BUILD_NNUE=OFF
cmake --build build-python --config Release --target _core
```

Exercise the distribution and existing tests in the intended Python environment:

```sh
python -m pip install ".[test]"
python -c "import pymoku; print(pymoku.__file__)"
python -m pytest python_bindings/tests/test_pymoku.py -q
```

Treat configure, extension link, package installation, import, and pytest as separate
verification stages; report the actual stage reached. Check the imported package path
so an unrelated installed copy cannot mask packaging changes. For wheel changes, build
and inspect a wheel, install it in an isolated environment, and import that artifact.

Reuse the existing pytest module for API regressions. Cover changed defaults, exceptions,
NumPy shape/dtype/copy behavior, state preservation, and lifetimes as appropriate.
Restore global settings changed by tests. Update registrations, package exports,
README examples, and these instructions together for API changes. Match the root
C++ formatter and nearby Python style; write documentation in technical English.