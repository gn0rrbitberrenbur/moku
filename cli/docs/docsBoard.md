# CLI board
## Purpose
Maintain the authoritative game position independently of engine state.

## Files
[board.hpp](../moku-cli/board/board.hpp), [board.cpp](../moku-cli/board/board.cpp)

## Interface
`Board(size, rule)` supports square boards from 5 to 19. `Stone` is Empty, Black or White.
`play` rejects out-of-range moves, occupied squares and Empty stones. `wins` scans both sides
of the last stone in the horizontal, vertical and two diagonal directions.

- `Freestyle`: a contiguous run of five or more wins.
- `ExactFive`: only a contiguous run of exactly five wins.
- `full`: all cells are occupied; the application checks wins before declaring a draw.

There are no Renju forbidden moves, Caro constraints, swap openings or undo operations.

## Coordinates
`Move{x,y}` is zero-based, with `x` increasing rightward and `y` increasing downward.
The user sees columns A-S and rows 1-19. `parseCoordinate` accepts case-insensitive coordinates
such as `H8`; `parseProtocolMove` accepts `7,7`. Both reject trailing junk and out-of-range values;
`parseProtocolMove` throws `std::out_of_range` for well-formed coordinates outside the board, so
engine workers can classify them as illegal moves.
Invalid human input is reported without changing the board. Invalid engine input forfeits the game.

## Validation
Tests cover all four winning directions, overlines under both rules, wins joining both sides
of the last move, occupied/out-of-range placements, full boards and malformed coordinates.