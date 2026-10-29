# nnue — Gomoku NNUE Dataset Generator

The source code in this repository generates self-play training data for a Gomoku NNUE. It uses the
[moku](https://github.com/gn0rrbitberrenbur/moku) engine (fetched at build
time) to play self-play games and records, for every position, feature planes plus
value and policy targets. Datasets are written as versioned binary shards for training.

## Requirements

- A C++17 compiler (MSVC, GCC, or Clang).
- CMake >= 3.14.
- Network access on the first configure (moku and googletest are fetched via
  `FetchContent`).

moku is pulled from `https://github.com/gn0rrbitberrenbur/moku.git`, tag `v0.0.5`,
and built as the static library `moku_core`.

## Build

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

## Run

```sh
nnue_gen [options]
```

| Option | Description | Default |
|---|---|---|
| `-n, --positions <N>` | target number of positions | `1000000` |
| `-s, --size <N>` | board size (5–19) | `15` |
| `-d, --depth <N>` | maximum search depth | `12` |
| `-t, --time <ms>` | search time per move (ms) | `20000` |
| `-c, --cores <N>` | worker threads | all cores |
| `-O, --opening <N>` | max. random opening plies (0 = off) | `8` |
| `-V, --val <F>` | validation game fraction (0–1) | `0.05` |
| `-o, --output <path>` | output path without `.bin` | `./nnue_dataset` |
| `-h, --help` | print usage and exit | — |

Example:

```sh
nnue_gen -n 100000 -s 15 -d 12 -t 20000 -o nnue_dataset
```

This writes training shards `nnue_dataset-00000.bin`, … and validation shards
`nnue_dataset_val-00000.bin`, … . Live progress (percent, rate, ETA) is printed to
stderr.

## Output format

Each shard begins with a `DatasetHeader` (magic `'GNN1'` = `0x314E4E47`, version `2`),
followed by packed samples. Per sample:

- `feature_len = 2 * board_size²` int8 planes in side-to-move view
  (plane 0 = own stones, plane 1 = opponent stones),
- `side_to_move` (u8), `ply` (u16),
- `search_score`, `final_value`, `td_value` (i16, scaled by `NNUE_SCORE_SCALE = 100`),
- `policy_count` (u8) and `NNUE_MAX_POLICY = 8` policy entries (u16 move + u16 prob).

Training and validation data use separate prefixes (`<path>-*.bin` and
`<path>_val-*.bin`).