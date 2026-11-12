---
description: "Use when editing NNUE self-play generation, value and policy targets, binary dataset shards, NumPy loading, or generator contract tests."
applyTo: "nnue/**"
---
# NNUE Dataset Generation Instructions

## Scope And Ownership

Apply `.github/copilot-instructions.md`. This area generates Gomoku training data
using the local C++ engine. Paths below are relative to `nnue/generate_dataset/`.

- `CMakeLists.txt`: targets `nnue_gen` and optional `nnue_tests`, linked to `moku::core`.
- `include/nnue.hpp`: configuration/defaults, dataset constants, sample/header types,
  generator interface, and output buffers.
- `src/main.cpp`: CLI parsing, dimension validation, generation and save sequence.
- `src/nnue.cpp`: openings, self-play workers, targets, feature encoding, shard writing.
- `src/quality.cpp`: tactical-safe move selection, canonical keys, opening-pool validation.
- `src/load_dataset.py`: NumPy reader, D4 augmentation, dense policies, sample rendering.
- `src/analyze_dataset.py`: score calibration, diversity, and train-validation quality reports.
- `tests/test_moku.cpp`: GoogleTest contracts for the local engine API.
- `tests/test_generation.cpp`: tactical selection, diversity keys, and generation failures.
- `README.md`: generator usage and dataset reference.

The root builds the engine from `engine/moku/`. Include its public headers as
`game/board.hpp`, `config.hpp`, `eval/evaluation.hpp`, and `search/minimax.hpp`.
Extend the contract tests when introducing a new core dependency.

## CLI And Configuration

| Option | Default | Meaning |
| --- | --- | --- |
| `-n`, `--positions` | 1000000 | Requested total stored positions |
| `-s`, `--size` | 15 | Board size, validated from 5 through 19 |
| `-d`, `--depth` | 12 | Maximum search depth |
| `-t`, `--time` | 20000 | Search budget per move in milliseconds |
| `-c`, `--cores` | Hardware concurrency, fallback 1 | Self-play worker count |
| `-O`, `--opening` | 8 | Maximum unstored random opening plies |
| `-V`, `--val` | 0.05 | Canonical opening hash-space fraction assigned to validation |
| `-o`, `--output` | Working-directory `nnue_dataset` | Output prefix |
| `-h`, `--help` | | Print usage and exit |

Quality defaults: seed 1234, split seed 2026, minimum opening length 2,
exploration before absolute ply 16 with probability 0.15, at most four candidates
within 200 raw score units of the best, raw sampling temperature 100, raw policy
temperature 200, normalization divisor 1000, minimum completed depth 1, sample
stride 2, maximum samples per game 32, TT budget 16777216 bytes per worker,
TD lambda 0.7, and shard size 1000000. Relabeling is disabled by default;
enabled relabeling uses maximum depth 14 and 80000 ms unless configured.
The CLI exposes these quality settings, `--opening-pool`, and seed controls;
the complete option table is in the module README.
Positions, depth, time, and worker count must be positive; opening maximum must be
nonnegative; validation fraction must be finite and in `[0,1]`. An opening maximum
below the minimum disables openings. Board dimensions are validated from 5 through 19.

## Self-Play And Target Contracts

- Set global board dimensions before launching workers and keep them stable throughout
  generation. Initialize the evaluator positional cache before launching workers.
  Board indices are row-major; black/white bitsets have capacity 361.
- Workers own agents, evaluators, boards, RNGs, and split buffers. Each uses timed
  single-thread search and clears its TT per game. Worker seeds are `seed + index`.
  Treat the evaluator's shared positional cache as synchronization-sensitive.
  Worker exceptions cancel generation, join all threads, and propagate to the CLI.
- Requested quotas divide across workers, with the remainder assigned to the last.
  Complete games before labeling and subsampling; only the final retained sample
  list may be truncated at quota. Samples accumulate in memory; worker buffers
  merge in worker order after joins.
- Openings are randomized and unstored. Stored samples describe the position before
  its selected move.   Selection always takes immediate wins and blocks a unique immediate opponent win.
  Otherwise early exploration uses raw STM score softmax over plausible candidates;
  later moves use the search best. Missing completed searches fail explicitly.
- Engine root/candidate scores are black-positive. Stored search values are
  `round(10000 * tanh(STM_root_score / score_norm))`; final values are STM results
  -10000/0/10000. Preserve raw best and played-move scores independently.
- Timed single-thread root candidate scores use independent full alpha-beta windows,
  so policy sampling and policy targets consume exact scores for the completed depth.
- TD targets blend quantized search values and the next TD value backward in
  black-positive space, starting from the final result, then convert to STM.
- Policy keeps at most eight best candidates and normalizes raw-score softmax over
  those retained moves. Omit probabilities rounding to zero and assign the rounding
  residual to the best move so stored probabilities sum to 65535.
  Candidate node counts are search statistics; score softmax determines policy weights.
- Canonical absolute-color D4 opening keys and split seed assign whole games to splits.
  Identical canonical openings keep the same split across runs with fixed split settings.
  Global full canonical position keys suppress symmetry duplicates across both splits
  within a run. A stride and random sample cap reduce within-game correlation.
  Stop explicitly after 1000 consecutive games without new samples.
- Optional higher-budget relabeling refreshes search/policy metadata before TD
  calculation without changing the played continuation or its final result.
- Maintain deterministic seed handling, state restoration, and target sign conventions
  when editing generation. Reuse buffers where practical in per-position hot paths.

## Binary Format And Python Reader

The writer serializes fields individually. Preserve this wire layout rather than
using C++ struct size/alignment as the disk format.

- Constants: magic `0x314E4E47`, version 3, score scale 10000, maximum policy count 8.
- The header is 48 bytes: `u32 magic`, `u32 version`, `u16 board_size`, `u16 rule`,
  `u32 sample_count`, `u16 feature_len`, `u16 score_scale`, `u16 max_policy`,
  `u16 reserved0`, and three reserved `u64` fields. Rule and reserved values are zero.
- Each record is `int8[feature_len]`, `u8 side_to_move`, `u16 ply`, three `i16`
  search/final/TD values, `u8 policy_count`, and eight `(u16 move, u16 probability)`
  entries, followed by game/opening/position uint64 IDs, two raw float32 scores,
  uint64 nodes, uint16 completed depth/played move/flags, uint8 end reason, and
  int8 tactical outcome. Record size is `2 * board_size^2 + 90` bytes; 15x15 uses 540 bytes.
- Features are two row-major binary planes: own stones, then opponent stones.
  STM byte 1 denotes black; 0 denotes white. Samples retain raw black/white bitsets
  until the writer materializes features. Probabilities use scale 65535.
- C++ requires little-endian IEEE-754 float32; Python reads little-endian fields.
  Update writer, reader,
  version constant, tests, and format documentation together for layout changes.
- Train files use `<prefix>-00000.bin`; validation uses `<prefix>_val-00000.bin`.
  Each split starts numbering at zero. Saving truncates current shard files and removes
  obsolete higher-numbered shards for that prefix. The parent directory must exist.
  `<prefix>.manifest.json` records configuration, engine/model identity and run counts.
- `load_shard(path)` returns arrays including `X:int8[N,2,B,B]`, STM/count `uint8`,
  ply/moves `uint16`, normalized values/probabilities `float32`, and `bs`/`scale`.
- The reader accepts version 3 only and validates metadata, record count/size,
  features, STM, target ranges, legal moves/probabilities, quality fields and games.
- `load_dataset(prefix)` accepts a file or loads sorted `prefix + '-*.bin'` matches
  eagerly. `augment`/`sym_permutation` transform both feature planes and valid policy
  moves through D4 symmetries; `policy_to_dense` maps stored policies to board indices.
  `render` and `show_examples` provide inspection output.

## Build And Verification

The project requires CMake >=3.15 and C++17. The verification commands below use
CMake/CTest >=3.20 for `--test-dir` support. Run them from the repository root:

```sh
cmake -S . -B build-nnue -DCMAKE_BUILD_TYPE=Release -DMOKU_BUILD_ENGINE=OFF -DMOKU_BUILD_TESTS=ON -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=ON
cmake --build build-nnue --config Release --target nnue_gen nnue_tests
ctest --test-dir build-nnue -C Release -R '^(DatasetQuality|MokuContract)\.' --output-on-failure --no-tests=error
```

`MOKU_BUILD_TESTS=ON` creates `nnue_tests`. Generator compilation and test execution
are separate operations; run the contracts explicitly. GoogleTest comes from the root
dependency helper at v1.14.0. Both targets use the repository warning helper.

The generator is under `build-nnue/nnue/generate_dataset/`; multi-configuration builds
add `Release/`. A small Windows multi-configuration smoke run is:

```powershell
.\build-nnue\nnue\generate_dataset\Release\nnue_gen.exe -n 10 -s 5 -d 2 -t 10 -c 1 -o nnue_smoke
```

For generation/format changes, also load smoke shards using `load_dataset.py` and
check header fields, sample count, shapes, target ranges, and policy indices. Contract
tests cover board/evaluator/search reuse; serialization and augmentation need their
own focused checks. Run reader contract tests with
`python -m unittest nnue.generate_dataset.tests.test_load_dataset nnue.generate_dataset.tests.test_analyze_dataset -v`. NumPy is required
for reader checks. The reader's script entry
point shows examples from the `nnue_dataset` prefix in the working directory.

Use root formatting/naming conventions and `#pragma once`. Keep function documentation
above definitions; preserve the local `@param name ; type ; description` and
`@return type ; description` format. Maintain the README, format reference, and these
instructions in technical English whenever their contracts are affected.