# Paired engine matches
This standalone Python 3.11+ tool compares two engine executables.

The engine executables run in separate processes. 

Each process has dedicated stdout and stderr reader threads. 

Only the engine whose turn it is receives a search command. 

One game runs at a time.

## Running a match
Install the benchmarks Python dependencies and run from the repository root:

```text
python -m venv benchmarking\.venv
.\benchmarking\.venv\Scripts\Activate
python -m pip install -r benchmarking/requirements.txt
python -m benchmarking.src benchmarking/example.toml
```

Edit [example.toml](example.toml) first:

- The first `[[engines]]` entry is the reference, the second is the candidate.
- Set distinct names, executable commands, and working directories.
- Commands are argument arrays, not shell command strings. Paths with spaces are
  single array elements. No shell is invoked.
- File paths and engine working directories resolve relative to the TOML file.
  An executable name without a path can also resolve through `PATH`. Other command
  arguments are passed unchanged and resolve according to the engine working directory.

Which engine plays black in the first game alternates between opening pairs. 

Each game starts fresh processes to isolate search caches and previous-game state. 

Processes exit after the game, subsequent games always start new ones, including after technical failures.

SPRT can stop after a complete pair. Without an SPRT decision, the maximum is 200 games, and the decision remains `inconclusive`.

There are no engine source changes or additional build dependencies.

## Configuration contract
All settings shown in the example are required except `run.max_nodes` and the
`technical` section. Unknown keys and invalid values produce errors before play.

| Section/key | Contract |
| --- | --- |
| `run.output_dir` | Parent directory for unique timestamp/UUID run folders |
| `run.openings` | JSON book containing exactly 100 valid, distinct openings |
| `run.move_time_ms` | Requested `INFO timeout_turn`, integer 1 through 86400000 |
| `run.timeout_tolerance_ms` | Additional external response allowance, 0 through 60000 ms |
| `run.startup_timeout_ms` | Deadline per initialization or display request, 1 through 300000 ms |
| `run.metrics_interval_ms` | Requested `METRICS` interval, 1 through 60000 ms |
| `run.depth` | Requested maximum search depth, 1 through 20 |
| `run.max_nodes` | Requested recursive node limit per search, integer 0 through 2147483647; optional, defaults to 0 (unlimited) |
| `technical.move_time_ms` | Requested time per technical search, 1 through 86400000 ms; defaults to 1000 |
| `technical.depth` | Requested maximum depth per technical search, 1 through 20; defaults to 12 |
| `technical.repetitions` | Repetitions per position and engine, 1 through 100; defaults to 5 |
| `engines.name` | Distinct, nonempty engine label |
| `engines.command` | Nonempty argument array; executable must resolve |
| `engines.cwd` | Existing engine working directory |
| `sprt.enabled` | Enable pair-boundary early stopping |
| `sprt.elo0`, `sprt.elo1` | Logistic Elo hypotheses, -1000 through 1000, with `elo0 < elo1` |
| `sprt.alpha`, `sprt.beta` | Nominal error rates, each strictly between 0 and 0.5 |
| `sprt.min_pairs` | Minimum complete opening pairs before a decision, 1 through 100 |

Both engines receive identical requested time, depth, node limit, and metric settings.
The runner sends `INFO max_nodes` during every engine initialization, including
preflight and each game. In the current extended moku engine, a positive limit
caps recursive minimax node visits across all workers per search; root candidate
generation and evaluation are excluded. Time, depth, and search-specific stopping
conditions still apply and can stop a search before the node limit is reached.
`INFO` has no acknowledgment, so compatibility checks do not prove that another
engine honors the requested limit.

## Technical benchmark

After the paired games, both engines search the same six deterministic 15x15
positions: early, attacking and defensive threats, clustered and dispersed
midgames, and edge interactions. These exercise different workloads; they do not
prove that a particular optimization was triggered. Each search starts a fresh
process with a cold transposition table. The order of the engines alternates
between repetitions. The technical section sets its own time and depth; the
runner sends `INFO max_nodes 0` regardless of `run.max_nodes`. It uses the
configured startup timeout and move timeout tolerance from `[run]`. The external
response deadline and engine-specific stopping conditions still apply.

The report compares per-position medians for elapsed time, nodes, nodes per
second, TT hits, completed depth, TT capacity and probes, alpha-beta cutoffs,
and root candidates. A comparison uses only repetitions where both engines
finished and reported that metric. Missing values and percentages with a zero
reference are `n/a`. Graphs show pooled matched medians; these numbers describe
the chosen positions and machine, not universal speed or playing strength.
The benchmark runs the `BOARD` search path; it does not measure `TURN`/SMP
or warm-table reuse. Engines can implement or count metrics differently, and
the protocol does not confirm that an engine honors the requested limits.

Engine failures, timeouts, illegal moves, and absent or ambiguous final metric
snapshots are recorded per observation and the remaining measurements continue.
The Markdown report lists those errors. File, logging, and cleanup errors abort
the run rather than being presented as valid observations.

The wall-clock response deadline includes the requested move time plus tolerance, starting before the move request is sent. 

The tolerance is **not** extra requested search time. 

Startup and process-shutdown time are outside the move budget.

Engine implementations determine the actual search budget and internal parallelism.

For example, the current moku handlers subtract a reserve from positive turn limits
and apply a minimum search budget. 

They also use different search entry points for `BOARD` and `TURN`. 

The runner does not override those implementation choices.

The protocol exposes no CPU-thread-count setting. Reader threads do not restrict engine SMP threads.

## Openings and rules
[openings.json](openings.json) contains a committed synthetic book generated locally
by [openings.py](openings.py), with generator ID `moku-local-v1` and seed `20261103`.
There is no external opening-book dependency or copied opening dataset.

Generation starts with black at `(7, 7)`. Seven further alternating moves are
selected uniformly from the ordered candidate list: unoccupied squares in the
central 9x9 region, within Chebyshev distance two of an existing stone. Positions
containing a line of four or more for either color are rejected. Duplicates are
removed under all eight square symmetries, ignoring move order within each color.
The first 100 accepted positions form the book. These positions are legal
diverse test inputs, **not an engine-calibrated or strength-balanced book**.

Generate a separate copy for inspection:

```text
python -m benchmarking --generate-openings openings-check.json
```

Generation refuses to overwrite an existing file. The committed book, rather than
the behavior of future Python random implementations, defines the fixed suite.
The loader checks coordinates, alternating turns, unique IDs, symmetric duplicates,
and absence of wins throughout each opening. Custom books can contain other
nonempty opening lengths, subject to the same validation and the 100-opening count.

Rules are 15x15 freestyle: black starts, colors alternate, five **or more** connected
stones in any of four directions win, and a full board without a win is a draw.
There is no Renju, Swap2, evaluation-based adjudication, or move-count draw cutoff.
The runner, not an engine score, determines the result.

## Protocol compatibility and failures
Before any scored game, both engines pass a logged compatibility check:

1. `START 15`, `ABOUT`, time/depth/node/rule/game-type settings, and `METRICS interval`.
2. `BOARD` probes for black-to-move and white-to-move positions.
3. Legal returned moves and a `DISPLAY` board matching the expected stone colors.

The required display format is moku's numbered `X`/`O`/`.` board followed by
`DEBUG White stones:`. Required protocol errors, unexpected replies, display
mismatches, crashes, and probe timeouts abort the run with a nonzero exit code.
Preflight detects these exercised incompatibilities; it cannot prove all engine
behavior or guarantee that an older binary honors every accepted setting.
Fast searches need not emit a progress metric. Current moku search entry points
emit a final `search_finished` metric whenever debug output is enabled.

Only the side to move receives a `BOARD` import. Fields `1` and `2` mean that
receiving engine's own and opponent stones, not fixed black and white identifiers.
The first request to each engine uses `BOARD`; subsequent requests use `TURN`.

During scored games, a crash, invalid or occupied move, unexpected protocol
response, or response timeout loses the game for the responsible engine.
Technical games participate in the primary match score and are also broken out
separately. This measures playing performance including reliability. The
`nontechnical` breakdown isolates ordinary game outcomes without claiming a
separate statistically unbiased strength estimate.

An orderly shutdown sends `END`. A process that does not exit is terminated and
then killed after bounded waits. Pipe readers drain before the JSON file closes.
Shutdown and logging failures abort the run rather than silently dropping data.
The runner manages the launched engine processes, not descendant process trees.
Use direct engine executables rather than wrappers that leave detached children.

### Notebook cancellation

[The live notebook](benchmark_runs/2026_11_05_v0_0_9.ipynb) starts the benchmark
in its plotting cell. Interrupting that cell sets a `threading.Event` and waits
for the benchmark thread to finish cleanup. Run the setup and preparation cells
before starting another match.

The Python API accepts an optional keyword-only `stop_event`:

```python
from threading import Event
from benchmarking.src.runner import RunCancelled, run

stop_event = Event()
# Pass the event to the worker; the notebook calls stop_event.set() on interruption.
try:
    folder = run(config, progress_path=metrics_path, stop_event=stop_event)
except RunCancelled:
    print("Benchmark cancelled; engine processes and logs are closed.")
```

The runner checks cancellation before starting engines, while sending protocol
commands, between games, and at intervals of at most 100 ms while waiting for
responses, including initialization and compatibility probes. Cancellation is
separate from an engine failure: the active game is interrupted, not scored as a
technical loss. Completed games remain in the summary, and only complete pairs
participate in Elo/SPRT calculations. The manifest and summary record
`status: "interrupted"`; the manifest records `stop_reason: "cancelled"`.
`RunCancelled` propagates after cleanup and log finalization.

Shutdown sends `END` even when cancellation is requested and uses the existing
bounded termination/kill waits. Cleanup can therefore take longer than the
cancellation polling interval. The caller must set the event in a `finally`
block and join its worker thread; interrupting a notebook cell alone does not
signal a background thread. A forced kernel termination, a second interruption
during cleanup, or detached descendants are outside this cooperative contract.
Calls without `stop_event` retain the normal CLI behavior.

## Output
Every invocation creates its own folder:

```text
<output_dir>/<UTC timestamp>-<unique suffix>/
    config.toml
    openings.json
    manifest.json
    preflight.json
    game-001.json
    game-002.json
    ...
    summary.json
    technical.json
    technical-0001.json
    ...
    report.md
    results.svg
    depth_completed.svg, nps.svg, elapsed_ms.svg, nodes_total.svg
    technical-nps.svg, technical-elapsed_ms.svg, technical-depth_completed.svg
```

- Configuration and opening files are snapshots. Relative paths in the copied
  TOML retain their original meaning; the manifest records the original config
  location, resolved commands, directories, and effective runner settings.
- The manifest records executable/config/book SHA-256 hashes, platform, Python
  version, engine `ABOUT` identities, run status, and stop reason. Hashes identify
  the executable itself, not arbitrary dependencies, wrappers, or argument files.
- Each game file contains opening/color metadata, an ordered event array, the
  complete move list, and its result.
- `technical.json` records the technical settings, positions, individual final
  metrics, failures and transcript names. Each `technical-NNNN.json` contains
  one search's protocol events and result. The summary of completed games is
  finalized even when the subsequent technical phase is interrupted.
- Every stdout/stderr line has decoded `text` and `raw_base64` containing the
  exact received bytes, including line endings or invalid UTF-8. Commands include
  the sent CRLF terminator. An unterminated final output fragment is also retained.
- Events identify engine, channel, sequence, monotonic nanoseconds since file
  creation, and the engine's currently observed requested ply. Initialization
  normally has a null ply. Move-acceptance, process lifecycle, EOF, and shutdown
  events are logged too.
- `[METRICS]` JSON is parsed into a `metrics` field in addition to raw capture.
  All fields are retained without renaming or filtering, for both
  `search_progress` and `search_finished` events.
  Malformed/nonfinite metric JSON retains its raw output and records
  `metrics_parse_error`.
- Separate stdout and stderr pipes have no shared causal ordering. Timestamps,
  sequence numbers, and ply tags describe observed receipt and request context,
  not proof that a late diagnostic belongs causally to that ply.
- The summary updates after each complete pair and on normal exit, exception,
  or Ctrl+C. It records W/D/L, color and technical breakdowns, paired statistics,
  and game results. An unmatched completed game is listed but excluded from
  paired Elo/SPRT calculations.
- After normal completion, `report.md` and its SVG charts summarize paired results,
  Elo uncertainty and SPRT, color and technical outcomes, per-engine median search
  metrics, and the probe-weighted TT hit rate. Only final metric snapshots from
  completed game logs are used; preflight and progress snapshots are excluded.
  Missing metrics are displayed as `n/a`, with sample counts. Charts for metrics
  with no samples are omitted. SVG files must remain beside the Markdown file.
  Matplotlib generates the SVG charts without opening a graphical window.
  The runner checks for Matplotlib before starting a match. If report generation fails, the error is
  surfaced and the completed match's manifest, summary, and game logs remain.
  Interrupted and failed runs retain their logs and summary but do not automatically
  generate a report. A report can be generated from finalized files with
  `from benchmarking.src.create_report import create_report; create_report(Path("run-folder"))`
  after importing `Path` from `pathlib`.

Game JSON streams to disk and flushes each event, so memory use does not grow
with the entire log. The closing result is written after pipe drainage. Normal
exceptions and Ctrl+C finalize the document. A power loss or forced termination
of the runner can leave the active game JSON incomplete; earlier closed games
and atomically replaced summaries remain available. Automatic resume is not
implemented. Exit codes are 0 for a completed run (including inconclusive SPRT),
1 for handled configuration/runtime failures, 2 for CLI usage errors, and 130
for Ctrl+C.

### Search metrics
Current moku debug output includes these fields in every progress/final snapshot:

| Field | Meaning |
| --- | --- |
| `tt_capacity_entries` | Number of allocated TT slots |
| `tt_allocated_bytes` | Allocated entry-vector bytes, excluding metadata and allocator overhead |
| `tt_occupancy_permille` | Occupied slots as integer permille, rounded down |
| `tt_probes_total` | TT lookup attempts during this search |
| `tt_hits_total` | Matching occupied TT entries during this search |
| `depth_completed` | Deepest fully completed iteration from any search worker |
| `workers_active` | Currently executing workers; zero in final snapshots |
| `alpha_beta_cutoffs_total` | Recursive closed-window cutoffs, including TT-bound cutoffs |
| `root_candidates_count` | Generated root candidate count before search |

Counters aggregate SMP workers and reset per search. TT contents and occupancy
persist across moves in the same engine process. The runner logs observations,
not sums of cumulative snapshots; it does not add these fields to Elo/SPRT
statistics or synthesize missing metrics from older engines. Existing timing,
node, current-depth, best-move, and score fields remain available.
See the [search reference](../engine/docs/docsSearch.md) for exact engine semantics.

## Strength statistics
All results use **candidate minus reference**. A game scores 1 for a candidate
win, 0.5 for a draw, and 0 for a loss. With mean score `s`:

```text
delta_elo = 400 * log10(s / (1 - s))
```

Zero and perfect scores produce the JSON strings `"-infinity"` and `"+infinity"`,
not invalid JSON numbers or invented finite ratings. No observations produce null
point estimates.

### 95% interval with early stopping

The normalized pair score is `(game_1_score + game_2_score) / 2`, bounded in `[0, 1]`.
For pair count `n`, the two-sided Hoeffding interval spends
`alpha_n = 0.05 / (n * (n + 1))` and uses score radius
`sqrt(log(2 / alpha_n) / (2 * n))`. Clipped score endpoints are transformed to Elo.
Because the alpha spending sums to 0.05, this conservative confidence sequence
remains valid at a data-dependent stopping time, under independent bounded pair
observations. Correlation between the two games within a pair is allowed.

This interval is deliberately wider than an ordinary fixed-sample interval.
The fixed synthetic book is not a random sample of all Gomoku openings; a match
cannot establish a universal rating. Repeated deterministic games do not create
independent new evidence. Results concern the selected openings, builds, machine,
and search settings.

### Pentanomial SPRT

The two game scores in a pair sum to `0, 0.5, 1, 1.5, 2`. The corresponding five
counts, in that order, form `pentanomial_counts`.
For each configured Elo hypothesis, a multinomial maximum-likelihood fit constrains
the expected normalized pair score to that Elo's logistic expected score. A
`1e-9` pseudocount per bin stabilizes empty categories. The generalized log
likelihood ratio is `sum(count_i * log(p1_i / p0_i))`.

The nominal Wald boundaries are:

```text
lower = log(beta / (1 - alpha))
upper = log((1 - beta) / alpha)
```

After at least `min_pairs`, reaching the lower boundary reports `accept_h0`;
reaching the upper reports `accept_h1`. Other results are `inconclusive`,
or `disabled` when SPRT is off. These are decisions between the configured
hypotheses, not proof of an exact Elo gain or absence of improvement.
Composite-model likelihood fitting, tiny regularization, and finite samples make
the stated SPRT error rates nominal approximations, not exact guarantees.

The example uses 0 versus +10 Elo and alpha/beta 0.05. Only 100 pairs often provide
insufficient evidence for small changes; an inconclusive result is expected and
is not a failure. Settings used merely to exercise early-stop behavior are not
meaningful strength-test parameters.