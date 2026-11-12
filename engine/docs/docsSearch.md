# Search Module Reference
`MinimaxAgent` selects a move using black-positive minimax scores, alpha-beta
pruning, heuristic move ordering, and a Zobrist-hashed transposition table.
Timed entry points add iterative deepening; an optional Lazy SMP path runs
multiple workers.

## 1. Public Interface
| API | Purpose |
| --- | --- |
| `MinimaxAgent(depth, max_memory_bytes = 0)` | Constructs an agent with a depth limit and optional TT byte budget; zero uses default capacity |
| `get_best_move(board, is_black)` | Fixed-depth search |
| `get_best_move_timed(board, is_black, time_limit_ms)` | Single-threaded iterative deepening |
| `get_best_move_timed_smp(board, is_black, time_limit_ms)` | Parallel iterative deepening, or single-thread fallback |
| `set_max_depth(depth)` | Updates the agent depth if the value is positive |
| `get_max_depth()` | Returns the configured limit, not the completed search depth |
| `get_completed_depth()` | Returns the deepest completed root iteration of the last search, or zero when none completes |
| `get_last_root_score()` | Returns the last score published by a supporting path |
| `get_last_root_candidates()` | Returns the stored root candidate vector |
| `get_nodes_searched()` | Returns the recursive node counter |
| `get_tt_hits()` | Returns the transposition-table hit counter |
| `clear_tt()` | Resets table entries without releasing table capacity |
| `set_tt_max_memory(max_memory_bytes)` | Applies a TT byte budget with no search in progress; a capacity change clears entries |
| `get_tt_size()` | Returns current TT slot capacity |
| `get_tt_allocated_bytes()` | Returns current TT entry-vector allocation in bytes |

Move-selection methods return an index or `-1` if no candidates exist. They
temporarily mutate the supplied board using make/undo; the caller applies
the selected move after return. Black maximizes scores and white minimizes them.
Use a positive constructor depth; the constructor does not validate it.

## 2. Recursive Search
The recursive routine:

1. Checks cancellation and periodically checks elapsed time.
2. Probes the transposition table and applies sufficiently deep exact values
   or bounds.
3. Evaluates the board at depth zero.
4. Generates nearby candidates and orders a cached move before heuristic moves.
5. Applies a move, checks `wins_at()`, searches the child if needed, and undoes it.
6. Updates alpha or beta and stops when the window closes.
7. Stores an exact score or bound unless the search was aborted.

An empty candidate list at an internal node returns zero. Move wins use scores
near `+100000` for black and `-100000` for white, with a depth-related adjustment.
Leaf evaluation uses the handcrafted evaluator rather than a learned model.

The fixed-depth root path orders by center proximity and enters recursion
directly. The timed roots use tactical priorities and explicitly detect wins
after root moves. These entry points are therefore not identical wrappers.

## 3. Timed Iterative Deepening
The single-threaded timed path searches depths 1 through the configured maximum.
Only a completed iteration replaces the committed move and root candidates.
If the first iteration cannot finish, the first generated candidate remains
the fallback. A sole candidate is searched like any other root move so its
published score reflects the position rather than a synthetic zero.

Time is checked at root moves and approximately every 1000 recursive nodes.
The check is not a hard real-time deadline. After a completed iteration, the
implementation also stops before starting another if:

```text
elapsed_milliseconds * 10 > requested_time_limit_ms
```

This is an early-stop heuristic, not a promise to use the whole budget.
A score with absolute value at least 90000 also stops iterative deepening.
Protocol handlers derive their budgets separately; see
[configuration](docsConfig.md).

`g_config.max_nodes` applies to fixed-depth and timed searches. Zero means no
limit; positive values cap recursive minimax node visits, excluding root move
generation and root candidates. SMP workers share the same atomic budget. If a
limit interrupts a search, timed search keeps the best move from the last
completed iteration, or its initial legal-move fallback.

## 4. Lazy SMP
The SMP entry point reads `g_config.cores`. Values of zero or one delegate to
the single-threaded timed path. Otherwise it creates helper threads and runs
one worker on the calling thread.

Each worker has its own board and root-move vector but shares the agent,
table, counters, timing, and cancellation state. Worker starting depths are
staggered between 1 and 2. A completed iteration can publish its move when
its depth exceeds the shared completed depth. **Any worker**, not only worker
zero, can determine the returned move.

Workers are joined before the call returns. This path does not populate
`last_root_score` or `last_root_candidates` as the single-threaded timed path does.

TT probe/store operations use striped mutexes; search counters and progress
gauges use atomics. Global configuration and the evaluator's positional cache
still require stable board dimensions and exclusive configuration changes.

## 5. Root Results and Statistics
`RootCandidate` contains:

| Field | Meaning |
| --- | --- |
| `move` | Root move index |
| `score` | Black-positive search result |
| `nodes` | Recursive nodes visited while searching this root move |

The candidate vector is produced by completed single-threaded timed iterations.
Each candidate is searched with an independent full alpha-beta window. During
these searches, only exact transposition-table entries at the same depth are
reused. Candidate scores therefore do not depend on bounds or deeper results from
another root line. Node counts are a visit proxy, not Monte Carlo visits. The
vector is not a sorted probability distribution.

Fixed-depth search publishes the root score but not a fresh candidate vector.
The SMP path does not refresh either result field. Search counters reset on
every entry point, including early returns. Table hits include successful probes
even when the cached depth is insufficient for a cutoff.

## 6. Debug Metrics
With `g_config.debug_output` enabled, all search entry points write
`[METRICS] ` followed by one JSON object per line to stderr. Progress is checked
approximately every 1000 recursive nodes and rate-limited by
`g_config.metrics_logging_interval`. Every search call also emits a
`search_finished` event, bypassing that interval, including empty and
sole-candidate returns. Periodic events use `search_progress`.

| Field | Contract |
| --- | --- |
| `tt_capacity_entries` | Allocated TT slots; defaults to 8388608 and reflects the extended `INFO max_memory` budget |
| `tt_allocated_bytes` | TT vector capacity times `sizeof(TTEntry)`; excludes object, locks, Zobrist arrays, and allocator overhead |
| `tt_occupancy_permille` | Integer floor of occupied slots times 1000 divided by slot count; range 0 through 1000 |
| `tt_probes_total` | Probe attempts since this search began, aggregated across workers |
| `tt_hits_total` | Matching occupied entries found since this search began, regardless of cached depth |
| `depth_completed` | Deepest fully completed root iteration; in SMP, from any worker |
| `workers_active` | Workers currently executing search; zero in the final event |
| `alpha_beta_cutoffs_total` | Closed-window cutoffs in recursive move loops and after applying TT bounds; exact-value TT returns are excluded |
| `root_candidates_count` | Number of generated root candidates before search, independent of the stored root result vector |

TT contents and occupancy persist between searches until `clear_tt()` or a
capacity-changing `set_tt_max_memory()`. The extended engine exposes the latter
through [INFO max_memory](docsExtendedProtocol.md#info-max_memory).
Other counters and gauges reset per search. The SMP timed path's sole-candidate
early return performs no iteration and reports completed depth zero. The
single-thread timed path searches a sole candidate normally. A fixed-depth search
reports its configured depth only after all root candidates finish.

Existing fields remain: `elapsed_ms` (minimum 1), `nodes_total`, `nps`,
`depth_current`, `best_move` (`{"x":...,"y":...}` or null), and `score` (number
or null). `depth_current` tracks the attempted iteration of worker zero in SMP;
another worker can complete a deeper iteration. Score is the latest worker-zero
progress score in SMP, not necessarily the returned worker's score.
Live atomic snapshots are observational, not a transaction over all counters.

The [Python match runner](../../benchmarking/README.md) retains each metric object
and its original output bytes.

## 7. Usage Example
```cpp
#include "config.hpp"
#include "search/minimax.hpp"

g_config.board_size = 15;
Board board;
board.make_move(board.pos(7, 7), true);
MinimaxAgent agent(6);

const int move = agent.get_best_move_timed(board, false, 1000);
if (move >= 0)
{
    board.make_move(move, false);
}
```

Configure board size before constructing the agent: its table initializes
Zobrist keys using the active dimensions. Do not share an agent across unrelated
concurrent search calls or resize global dimensions during a search.

## 8. Implementation Locations
- [minimax.hpp](../moku/search/minimax.hpp): agent interface and root result type.
- [minimax.cpp](../moku/search/minimax.cpp): fixed, timed, recursive, and SMP search.
- [Evaluation reference](docsEvaluation.md): candidate and score behavior.
- [Transposition table reference](docsTranspositionTable.md): cached bounds and hashing.