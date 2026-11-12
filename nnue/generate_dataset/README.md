# Gomoku NNUE Dataset Generator

The generator uses the local handcrafted moku engine for freestyle self-play.
It writes versioned feature, value, policy, and search-quality records for NNUE
experiments. Training and learned-model inference are separate components.

## Build and run

Configure from the repository root with CMake >= 3.15 and a C++17 compiler.
GoogleTest is required only when tests are enabled; the engine core is local.
The NumPy reader and quality analyzer require NumPy.

```powershell
cmake --build build --config Release --target nnue_gen nnue_tests
ctest --test-dir build -C Release -R "^(DatasetQuality|MokuContract)\." --output-on-failure --no-tests=error
python -m unittest nnue.generate_dataset.tests.test_load_dataset nnue.generate_dataset.tests.test_analyze_dataset -v

New-Item -ItemType Directory -Force nnue\datasets | Out-Null
.\build\nnue\generate_dataset\nnue_gen.exe -n 10000 -s 15 -d 6 -t 1000 -c 2 --seed 1234 -o nnue\datasets\run1234
python nnue\generate_dataset\src\analyze_dataset.py nnue\datasets\run1234 --output nnue\datasets\run1234.quality.json
```

The build commands assume an existing root build with `MOKU_BUILD_NNUE=ON` and
`MOKU_BUILD_TESTS=ON`. Multi-configuration generators add `Release` to the executable
directory. A clean NNUE-only configuration is:

```powershell
cmake -S . -B build-nnue -DMOKU_BUILD_ENGINE=OFF -DMOKU_BUILD_CLI=OFF -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=ON -DMOKU_BUILD_TESTS=ON
```

## Options

| Option | Meaning | Default |
| --- | --- | --- |
| `-n`, `--positions` | Total stored positions across both splits | 1000000 |
| `-s`, `--size` | Board size, 5 through 19 | 15 |
| `-d`, `--depth` | Maximum self-play search depth | 12 |
| `-t`, `--time` | Self-play search budget in milliseconds | 20000 |
| `-c`, `--cores` | Independent self-play workers | Hardware concurrency |
| `-O`, `--opening` | Maximum unstored random opening length; below 2 disables random openings | 8 |
| `-V`, `--val` | Fraction of canonical opening hash space assigned to validation | 0.05 |
| `-o`, `--output` | Output prefix; parent directory must exist | Working-directory `nnue_dataset` |
| `--seed` | Self-play RNG seed, uint32 | 1234 |
| `--split-seed` | Opening-based split seed, uint32 | 2026 |
| `--opening-pool` | Optional text file of validated move sequences | None |
| `--min-depth` | Required completed depth; generation fails if unmet | 1 |
| `--explore-plies` | Explore only before this absolute ply | 16 |
| `--explore-prob` | Probability of attempting exploration | 0.15 |
| `--explore-top-k` | Maximum plausible alternatives | 4 |
| `--explore-max-loss` | Maximum raw STM score loss relative to the best candidate | 200 |
| `--sampling-temperature` | Raw-score softmax scale for exploration | 100 |
| `--policy-temperature` | Raw-score softmax scale for top-eight policy targets | 200 |
| `--score-norm` | Divisor in `tanh(raw_STM_score / divisor)` | 1000 |
| `--sample-stride` | Retain every Nth position with randomized game offset | 2 |
| `--max-samples-per-game` | Randomly subsample at most this many positions per game | 32 |
| `--relabel-every` | Relabel every Nth post-opening position; zero disables | 0 |
| `--relabel-depth` | Maximum depth for relabeling | 14 |
| `--relabel-time` | Millisecond budget for relabeling | 80000 |
| `--tt-memory` | Per-worker TT entry-storage budget; zero uses engine default | 16777216 |
| `-h`, `--help` | Print usage | |

Budgets, depths, stride, sample cap, candidate limit, and temperatures are positive.
Probabilities are finite and in `[0,1]`; maximum loss is finite and nonnegative.
Opening length is nonnegative and at most the board area. Relabel budgets are at
least the self-play budgets when relabeling is enabled. Required depth applies to
all recorded searches, including searches that stop early after finding a win.
TT budgets exclude other engine memory and transient allocations.

## Move selection and targets

Every stored position receives a completed single-thread root search. Missing
completed iterations, insufficient depth, and invalid search results fail the run
explicitly rather than producing synthetic zero labels.

Before exploration, the generator checks all empty squares:

1. Play an immediate own win if available.
2. Block the opponent's unique immediate winning square.
3. If multiple opponent winning squares remain and no own immediate win exists,
   mark the position as an unavoidable immediate loss and play the search best.
4. Otherwise, occasionally sample among at most `explore_top_k` candidates within
   `explore_max_loss` raw units of the best. Beyond `explore_plies`, play the best.

The one-ply safeguards apply to stored self-play, not the unstored randomized
opening prefix. Search values and candidate values use raw STM scores for ranking
and sampling, independently of the bounded training-value transformation.

`search` is the normalized best root value. `played_score` is the raw score of the
actually selected move. `final` is the result of the completed continuation from
the sample's STM perspective. These are different signals, not interchangeable
claims about perfect play. `td` recursively blends search values with the next
TD value in black-positive space, starting from the actual game result; lambda
is 0.7. Quantized targets use scale 10000.

Optional relabeling reconstructs selected positions after the game and searches
with the higher budgets. It refreshes search/policy/played-score quality fields
and TD targets without changing the played continuation or its result. The
`EXPLORED` flag continues to describe the original self-play decision.
`RELABELLED` identifies the higher-budget targets. More budget does not guarantee
a deeper completed iteration or a stronger target.

## Openings, diversity, and validation

An opening-pool line contains whitespace-separated row-major indices, alternating
black and white from an empty board. Blank lines and `#` comments are accepted:

```text
112 113 127 128
111 126 112 127
```

The loader rejects malformed, repeated, out-of-range, terminal, and full-board
openings. It validates legality, not strategic balance. A supplied pool replaces
random opening generation. Otherwise 2 through `opening_max` unstored random plies
are used, with a central first move and subsequent nearby moves.

All eight D4 transformations of the absolute-color opening board share a canonical
key. Its hash with `split_seed` assigns the entire game to one split. Keep
`split_seed` and `val` fixed across runs for stable opening assignments. With only
one canonical opening, including `--opening 0`, all games go to the same split.
A small pool need not approximate the requested validation fraction.

Canonical position keys remove D4-equivalent repeats across both splits within a
run. Equality uses full keys, not only hashes. Transpositions across different
openings can still overlap between separate runs: run the analyzer before combining
datasets, and keep a fixed external holdout for model selection.

Sampling uses a random stride offset and per-game cap; games are played to their
actual end before sample quotas are applied. A final game's retained sample list
may be shortened to reach the exact requested total. Generation fails after 1000
consecutive games without new samples. Worker seeds are `seed + worker_index`;
game IDs are `(seed << 32) | worker_game_index`. Use distinct seeds when merging
runs. Timed searches and concurrent duplicate ownership are not bitwise reproducible.

## Binary format

Version **3** is supported; version 2 is rejected. Scalar fields are little-endian;
raw scores use IEEE-754 float32. The writer explicitly rejects unsupported hosts.
The packed 48-byte header is:

```text
u32 magic=0x314E4E47, u32 version=3, u16 board_size, u16 rule=0,
u32 sample_count, u16 feature_len=2*board_size^2, u16 score_scale=10000,
u16 max_policy=8, u16 reserved0=0, u64 reserved[3]={0,0,0}
```

Each record is `2*board_size^2 + 90` bytes, or 540 bytes for 15x15:

```text
i8 features[feature_len]             own stones then opponent stones, STM view
u8 side_to_move                     1 black, 0 white
u16 ply
i16 search, final, td                normalized targets times 10000
u8 policy_count
(u16 move, u16 probability)[8]       top-eight softmax, probability scale 65535
u64 game_id, opening_id, position_key
f32 raw_search, played_score         raw STM scores
u64 nodes                           nodes in the search supplying the target
u16 depth, played_move, flags
u8 end_reason                       1 five in a row, 2 full board
i8 tactical                         1 immediate own win, -1 unavoidable immediate loss, 0 unknown
```

Flags: `SEARCH_VALID=1`, `EXPLORED=2`, `IMMEDIATE_WIN=4`,
`FORCED_DEFENSE=8`, `RELABELLED=16`. Tactical labels describe verified one-ply
facts, not heuristic score thresholds.

Policy entries rounding to zero are omitted. The best entry receives the rounding
residual so active uint16 probabilities sum to exactly 65535.

The canonical key is board-size byte, absolute STM byte, and lexicographically
minimal D4 absolute-color board bytes (0 empty, 1 black, 2 white). IDs use FNV-1a
64 with offset 14695981039346656037 and prime 1099511628211.

## Output and quality analysis

Training shards use `<prefix>-00000.bin`; validation uses `<prefix>_val-00000.bin`.
`<prefix>.manifest.json` records configuration, supplied opening sequences, UTC
write time, engine version, handcrafted evaluator identity, null model identity,
generated game count, removed duplicates,
and stored sample counts. Reusing a prefix replaces its shards and manifest.
No shard is written for an empty split.

The reader validates format, record size, features, moves, probabilities, target
ranges, search quality, and game metadata. It returns normalized float targets,
raw scores, and the per-sample quality fields.

The analyzer reports sample/game/opening counts, outcome distributions, search
quality and score distributions, saturation, target residuals, score calibration
bins, exact and symmetry duplicate rates, and train-validation overlap. Reports
measure the generated sample; they do not prove playing-strength improvement.
Use score bins and saturation to choose a normalization scale experimentally;
the analyzer does not silently recalibrate targets.
