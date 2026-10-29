# Search Module Reference
`MinimaxAgent` selects a move using black-positive minimax scores, alpha-beta
pruning, heuristic move ordering, and a Zobrist-hashed transposition table.
Timed entry points add iterative deepening; an optional Lazy SMP path runs
multiple workers.

## 1. Public Interface
| API | Purpose |
| --- | --- |
| `MinimaxAgent(depth)` | Constructs an agent with an agent-local depth limit |
| `get_best_move(board, is_black)` | Fixed-depth search |
| `get_best_move_timed(board, is_black, time_limit_ms)` | Single-threaded iterative deepening |
| `get_best_move_timed_smp(board, is_black, time_limit_ms)` | Parallel iterative deepening, or single-thread fallback |
| `set_max_depth(depth)` | Updates the agent depth if the value is positive |
| `get_max_depth()` | Returns the configured limit, not the completed search depth |
| `get_last_root_score()` | Returns the last score published by a supporting path |
| `get_last_root_candidates()` | Returns the stored root candidate vector |
| `get_nodes_searched()` | Returns the recursive node counter |
| `get_tt_hits()` | Returns the transposition-table hit counter |
| `clear_tt()` | Resets table entries without releasing table capacity |

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
the fallback. A sole candidate returns immediately.

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

**Concurrency limitation:** Table entries are ordinary objects accessed without
locking or atomics, and the recursive `current_depth` member is also written
by multiple workers. Do not describe the current SMP implementation as
data-race-free. Use `cores = 1` when avoiding this parallel path is required.

## 5. Root Results and Statistics
`RootCandidate` contains:

| Field | Meaning |
| --- | --- |
| `move` | Root move index |
| `score` | Black-positive search result |
| `nodes` | Recursive nodes visited while searching this root move |

The candidate vector is produced by completed single-threaded timed iterations.
Node counts are a visit proxy, not Monte Carlo visits. Root alpha-beta windows
can make some values bounds rather than independent exact full-window scores.
The vector is not a sorted probability distribution.

Fixed-depth search publishes the root score but not a fresh candidate vector.
The SMP path does not refresh either result field. Early returns can also
leave counters from a previous search, so do not assume every accessor is
fresh after every entry point. Table hits include successful probes even when
the cached depth is insufficient for a cutoff.

## 6. Usage Example
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

## 7. Implementation Locations
- [minimax.hpp](../moku/search/minimax.hpp): agent interface and root result type.
- [minimax.cpp](../moku/search/minimax.cpp): fixed, timed, recursive, and SMP search.
- [Evaluation reference](docsEvaluation.md): candidate and score behavior.
- [Transposition table reference](docsTranspositionTable.md): cached bounds and hashing.