---
applyTo: "nnue/generate_dataset/**"
---
# Project Context
nnue is a standalone self-play **dataset generator** for a Gomoku NNUE (Efficiently
Updatable Neural Network). It drives the moku Minimax engine (fetched via CMake
FetchContent) to play self-play games and records, per position, STM feature planes,
a search value, the final game value, a TD(lambda) value and a soft policy target.
Datasets are written as versioned binary shards and consumed in Python (NumPy) for
training. The C++ generator produces the data.

## Dependency on moku
- moku is fetched as a subproject from `https://github.com/gn0rrbitberrenbur/moku.git`,
  always with an specific tag (v.x.x.x), and built as the static library `moku_core`.
- nnue includes moku headers by their public name (`board.hpp`, `config.hpp`,
  `minimax/minimax.hpp`), resolved through `${moku_SOURCE_DIR}/include`.
- Only these moku symbols are used: `Board` (`make_move`, `undo_move`, `wins_at`,
  `black`/`white` bitsets), `MAX_SQUARES`, `g_config`, `Evaluator::get_valid_moves`,
  `MinimaxAgent` (`get_best_move_timed`, `get_last_root_candidates`,
  `get_last_root_score`, `clear_tt`), and `RootCandidate`.
- A GoogleTest contract suite (`tests/test_moku.cpp`) pins this API. It runs as a
  build-time gate before `nnue_gen` compiles; a failing test aborts the build.

## Language
- When writing docstrings or documentation, always use English.

## Structure
- `include/` – headers (`.hpp`), e.g. `include/nnue.hpp`
- `src/` – implementations (`.cpp`): `main.cpp` (CLI), `nnue.cpp` (generation + I/O)
- `tests/` – GoogleTest contract tests for the moku API
- Build system: CMake (>= 3.14, needs `FetchContent_MakeAvailable`)

## Board Representation (inherited from moku)
- Cells are `std::bitset<MAX_SQUARES>` for `black` and `white` on `Board`.
- Index mapping: `pos = row * g_config.board_size + col`.
- Board size is runtime-configurable via `g_config.board_size` (5–19).
- Samples store raw `black`/`white` bitsets; feature planes are materialized only on
  write, from the side-to-move perspective (plane 0 = own, plane 1 = opponent).

## Dataset Format
- Constants (in `include/nnue.hpp`): `NNUE_SCORE_SCALE = 100`,
  `NNUE_MAX_POLICY = 8`, `NNUE_DATASET_MAGIC = 0x314E4E47` ('GNN1'),
  `NNUE_DATASET_VERSION = 2`.
- Each shard file (`<prefix>-NNNNN.bin`) starts with a `DatasetHeader`, followed by
  packed samples: `feature_len` int8 planes (STM), `side_to_move` (u8), `ply` (u16),
  `search_score`, `final_value`, `td_value` (i16, scaled by `NNUE_SCORE_SCALE`),
  `policy_count` (u8) and `NNUE_MAX_POLICY` policy entries (u16 move + u16 prob each).
- Training and validation samples go to separate prefixes: `<path>-NNNNN.bin` and
  `<path>_val-NNNNN.bin`.
- Keep the C++ writer (`write_shards`) and the Python reader (`load_dataset.py`)
  byte-compatible; any layout change must update both plus the version constant.

## Running the Executable
Invocation: `nnue_gen [options]`. Generates the dataset and writes shards.

- `-n, --positions <N>` – target number of positions (default 1000000).
- `-s, --size <N>` – board size 5–19 (default 15).
- `-d, --depth <N>` – maximum search depth (default 12).
- `-t, --time <ms>` – search time per move in ms (default 20000).
- `-c, --cores <N>` – worker threads (default: all cores).
- `-O, --opening <N>` – max. random opening plies (default 8, 0 = off).
- `-V, --val <F>` – validation game fraction 0..1 (default 0.05).
- `-o, --output <path>` – output path without `.bin`.
- `-h, --help` – print usage and exit.

Example: `nnue_gen -n 100000 -s 15 -d 12 -t 20000 -o nnue_dataset`.

## Self-Play Generation
- Work is split across threads; each worker runs a thread-local `MinimaxAgent`.
- Early plies use randomized (unstored) opening moves for diversity; stored moves are
  sampled by softmax over root-candidate scores with a ply-decaying temperature.
- Value targets: `search_score` (engine root), `final_value` (game result),
  `td_value` (TD(lambda) blend). All are stored in side-to-move view.

## Build & Test
- Configure: `cmake -B build -DCMAKE_BUILD_TYPE=Release`
- Build everything: `cmake --build build --config Release`
- Build generator: `cmake --build build --config Release --target nnue_gen`
- Build/run contract tests: `cmake --build build --config Release --target moku_contract_tests`
- FetchContent needs network access on first configure (moku, googletest).
- The contract tests are a hard gate: `nnue_gen` only compiles after they pass.
- Compiler warnings are strict (`/W4` on MSVC, `-Wall -Wextra -pedantic` otherwise);
  generated code must compile warning-free.

## Python Side
- `load_dataset.py` requires NumPy. `python load_dataset.py` shows a few examples
  from `nnue_dataset`.
- Public helpers: `load_shard`, `load_dataset`, `augment` (D4 symmetry),
  `sym_permutation`, `policy_to_dense`, `render`, `show_examples`.

## Naming Conventions
- Functions and variables: `snake_case`.
- Classes and structs: `PascalCase`.
- Constants and macros: `UPPER_SNAKE_CASE`.
- No `using namespace std;`, especially not in headers.

## Comments
- Do NOT use inline comments inside function bodies.
- Describe all behavior, parameters, and return values in the function docstring above
  the definition, using the existing format:
  - A short description of what the function does.
  - `@param name ; type ; description` for each parameter.
  - `@return type ; description`.
- File-level docstrings at the top of a file are allowed to describe the file's purpose.

## Performance
- `encode_stm`, `generate_worker`, `build_policy`, `sample_move` are hot paths.
- Avoid heap allocations in the per-position inner loops; reuse preallocated buffers.
- Prefer `inline` for small hot-path helpers. Maintain const-correctness.

## Do / Don't
- Make small, surgical changes; do not rewrite whole files for minor edits.
- Keep new headers guarded (`#pragma once`).
- When adding new source files, register them in `CMakeLists.txt` for every relevant
  target (`nnue_gen`, `moku_contract_tests`).
- When adding a moku symbol dependency, extend `tests/test_moku.cpp` accordingly.

## Response Style
- Be concise. No filler, no preambles.
- Always return an explanation with the code.