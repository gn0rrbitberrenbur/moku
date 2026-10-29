# Evaluation Module Reference
`Evaluator` provides static position scores, nearby candidate moves, and tactical
move-ordering priorities. It is a handcrafted evaluator.

## 1. Score Convention and Public Interface
| Method | Result |
| --- | --- |
| `evaluate_board(board)` | `float` score; positive favors black, negative favors white |
| `evaluate_board_stm(board, black_to_move)` | Same score from the side-to-move perspective |
| `get_valid_moves(board)` | Vector of nearby empty candidate indices |
| `move_priority(board, move, is_black)` | Ordering priority for a hypothetical move |
| `position_score(pos)` | Center-proximity score |
| `score_window(board, pos, dx, dy, is_black)` | Gap-aware five-cell window score |
| `analyze_line_fast(...)` | `LineInfo` with consecutive count and open ends |
| `is_line_dead(..., count, is_black)` | Whether both boundaries of a run are blocked |
| `score_pattern(count, open_ends)` | Legacy consecutive-pattern score |
| `analyze_line(...)` | Compatibility wrapper around `analyze_line_fast()` |

Direction arguments use `dx` for columns and `dy` for rows. Evaluation scans
horizontal, vertical, and both diagonal orientations. `LineInfo` stores
`count` and `open_ends`; the private merged scan returns `LineEval` with
`window_score`, `consec`, and `open_ends`.

## 2. Position Evaluation
The main evaluator visits occupied squares and adds signed center-proximity
scores. For each direction, it skips a square if the preceding square belongs
to the same color, so solid runs are evaluated from their starts.

Its merged line scan computes:

1. A five-cell window score that allows internal gaps.
2. The length of the leading consecutive run.
3. The number of open boundaries around that run.

A window contributes zero if it extends beyond the board or contains an
opponent stone. Otherwise, the number of own stones selects its score:

| Own stones | 0 | 1 | 2 | 3 | 4 | 5 |
| --- | --- | --- | --- | --- | --- | --- |
| Window score | 0 | 1 | 10 | 120 | 1200 | 100000 |

Runs of at least four with an open end add a signed 30000 bonus. Runs of at
least three with an open end, excluding those already counted as fours,
contribute a signed 50 bonus. The position contribution is
`max(0, board_size - ManhattanDistanceToCenter)`.

These are heuristic units, not probabilities or centipawns. The main evaluation
does not simply sum the public legacy `score_pattern()` table, and it is not
a complete enumeration of all five-cell windows.

## 3. Candidate Generation and Ordering
On an empty board, `get_valid_moves()` returns only the center index.
Otherwise, it collects empty squares within offsets `-2` through `2` in both
coordinates around every stone. A bitset removes duplicates; the result is
returned in ascending index order.

Despite its name, this is a restricted candidate generator, not a full Gomoku
legality API. Distant empty squares are omitted, and Renju restrictions are
not checked.

`move_priority()` inspects consecutive neighbors on both sides of a candidate:

| Tactical condition | Own-pattern bonus | Opponent-pattern blocking bonus |
| --- | --- | --- |
| At least four neighboring stones | 100000 | 50000 |
| Three neighbors and at least one open boundary | 10000 | 5000 |
| Two neighbors and two open boundaries | 1000 | 500 |

Center proximity is also included. This priority is used for ordering; it is
not the minimax value of the move.

## 4. Usage Example
```cpp
#include "eval/evaluation.hpp"

Board board;
Evaluator evaluator;
const float black_score = evaluator.evaluate_board(board);
const float white_to_move_score = evaluator.evaluate_board_stm(board, false);
const std::vector<int> candidates = evaluator.get_valid_moves(board);
```

An empty default board scores zero and produces the center candidate 112.
The side-to-move helper only flips the score sign; it does not run a different
evaluation or imply that an NNUE model is loaded.

## 5. Constraints
The module assumes valid indices, dimensions, disjoint color bitsets, and sensible
pattern arguments. Public scoring helpers do not comprehensively validate input.
Center scores are cached in module-level arrays keyed by the configured size.
Do not change dimensions concurrently with evaluation.

No incremental evaluation state is maintained on `Board`. The search evaluates
positions through this interface and detects move wins separately.

## 6. Implementation Locations
- [evaluation.hpp](../moku/eval/evaluation.hpp): public APIs and line-result types.
- [evaluation.cpp](../moku/eval/evaluation.cpp): scoring tables, scans, and candidates.
- [Search reference](docsSearch.md): consumers of scores and move priorities.
- [Core tests](../moku/tests/tests.cpp): `EvaluatorTest` suite.