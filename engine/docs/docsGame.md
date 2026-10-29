# Game Module Reference
The game module represents Gomoku positions using two fixed-size bitsets and
provides move mutation, coordinate helpers, win detection, and text display.
It does not manage turn order, validate complete games, or enforce rule variants.

## 1. Representation
`Board` exposes two public members:

| Member | Type | Meaning |
| --- | --- | --- |
| `black` | `std::bitset<MAX_SQUARES>` | Squares occupied by black |
| `white` | `std::bitset<MAX_SQUARES>` | Squares occupied by white |

`MAX_SIZE` is 19 and `MAX_SQUARES` is 361. A newly constructed board has no stones.
The active dimensions are read from `g_config.board_size`; there is no per-board
size or stored side to move.

Only the first `board_size * board_size` bits describe the active board.
Coordinates use row-major layout:

```text
position = row * size + column
row      = position / size
column   = position % size
```

The protocol's `X,Y` corresponds to `column,row`. On a 15x15 board, `7,7` is
position 112 and algebraic square `H8`.

## 2. Public Interface
| Method | Behavior |
| --- | --- |
| `size()` | Returns the globally configured side length |
| `squares()` | Returns the globally configured number of squares |
| `pos(row, col)` | Converts coordinates to an index |
| `row(p)`, `col(p)` | Convert an index to coordinates |
| `in_bounds(row, col)` | Checks both coordinates against the active dimensions |
| `make_move(pos, is_black)` | Sets the selected color's bit |
| `undo_move(pos)` | Clears both colors at the index |
| `test_pos(pos)` | Reports whether either color occupies the square |
| `check_win()` | Checks whether either color has a line of at least five |
| `wins_at(move, is_black)` | Checks lines through a supplied move for one color |
| `output_board()` | Prints a human-readable board to stdout |

Mutation methods do not enforce occupancy, alternation, or active-board bounds.
Callers must maintain disjoint bitsets and valid indices. A storage-valid index
can still lie outside the active board. Some bitset accessors throw for indices
outside storage; others do not provide checked access.

## 3. Win Detection
`check_win()` scans occupied squares in four directions: horizontal, vertical,
diagonal down-right, and diagonal up-right. `wins_at()` counts connected stones
in both directions through the supplied move and is used after search moves.

Both methods accept five or more connected stones as a win. They do not implement
exact-five, forbidden Renju moves, Caro conditions, or continuous-play rules.
The `rule` configuration field does not change this behavior.

`wins_at()` assumes the move has already been placed for the selected color.
It is not a general legality check or a substitute for validating the center stone.

## 4. Usage Example
```cpp
#include "config.hpp"
#include "game/board.hpp"

// Configure dimensions before constructing positions or search objects.
g_config.board_size = 15;
Board board;
const int move = board.pos(7, 7);

if (board.in_bounds(7, 7) && !board.test_pos(move))
{
    board.make_move(move, true);
    const bool won = board.wins_at(move, true);
    (void)won;
    board.undo_move(move);
}
```

Search uses make/undo on the same board to avoid recursive board copies. Lazy SMP
creates a board copy per worker so that workers do not mutate the same position.

## 5. Display and Limitations
`output_board()` prints column letters beginning at `A`, row numbers beginning
at 1, `X` for black, `O` for white, and `.` for empty squares. Do not call it
in a strict Gomocup response path: the raw grid is not a standard protocol reply.

Use board sizes 5 through 19. Changing the global size reinterprets all existing
boards, so do not resize during an active search or while retaining old positions.
The board has no move history, winner identity field, or explicit draw state.

## 6. Implementation Locations
- [board.hpp](../moku/game/board.hpp): storage, constants, and public interface.
- [board.cpp](../moku/game/board.cpp): mutation, display, and win detection.
- [Configuration reference](docsConfig.md): global dimensions.
- [Core tests](../moku/tests/tests.cpp): `BoardTest` suite.