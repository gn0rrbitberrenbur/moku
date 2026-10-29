# Gomocup Protocol
This document describes communication between a Gomoku game manager and an AI engine,
along with the current implementation in moku.

## Reference 
[Gomoku AI protocol](https://plastovicka.github.io/protocl2en.htm)
by Petr Lastovicka. 
> **Last checked against the reference**: October 26, 2026.

## 1. Communication and Coordinates
The game manager starts the engine as a console process and communicates through two pipes:

- **stdin:** commands from the manager to the engine.
- **stdout:** protocol responses from the engine to the manager.
- **stderr:** local diagnostic output that is not treated as a protocol response.

A command normally occupies a single line. `BOARD` and `SWAP2BOARD` introduce
multiline data blocks terminated by `DONE`. According to the specification, the
manager sends CRLF line endings; responses may use CRLF, LF, or CR.
Output must be flushed promptly so that the manager does not wait for buffered
responses.

moku reads input line by line and flushes its responses. Command keywords are
case-insensitive. For reliable input, however, use the spelling shown here:
`INFO` keys are expected in lowercase, and a `BOARD` block must end with
uppercase `DONE`.

Coordinates are written as `X,Y` without spaces:

- `X` is the column, and `Y` is the row.
- Both values start at `0`.
- On a 15x15 board, both values range from `0` to `14`.
- Internally, moku uses the index `pos = Y * board_size + X`.

A normal move response contains only the two coordinates, for example `7,7`.
A win is not reported through a special move response; the manager is
responsible for managing the game.

## 2. Command Overview
The direction in this table is **manager to engine**.

| Command | Purpose | Expected response | Status in moku |
| --- | --- | --- | --- |
| `START size` | Initialize an empty square board | `OK` or `ERROR ...` | Implemented; see the board size warning below |
| `BEGIN` | Ask the engine to open the game | `X,Y` | Implemented |
| `TURN X,Y` | Report the opponent's move and request an engine move | `X,Y` or an error | Implemented |
| `BOARD` ... `DONE` | Load a complete position and request a move | `X,Y` or an error | Implemented with limitations |
| `INFO key value` | Provide settings or limits | None | Partially processed |
| `END` | Shut down the engine | None | Implemented |
| `ABOUT` | Request engine information | One line of key-value pairs | Implemented |
| `RESTART` | Prepare a new game with the same board size | `OK` | Implemented |
| `TAKEBACK X,Y` | Remove a stone | `OK` or an error | Implemented |
| `RECTSTART width,height` | Initialize a rectangular board | `OK` or an error | Rejected with `ERROR` |
| `PLAY X,Y` | Specify a move after `SUGGEST` | `X,Y` | Not implemented |
| `SWAP2BOARD` ... `DONE` | Negotiate a Swap2 opening | Depends on the opening phase | Not implemented |

The specification requires at least `START`, `BEGIN`, `INFO`, `BOARD`, `TURN`, and
`END`. Unknown or unimplemented commands receive an `UNKNOWN` response without
causing the engine to exit.

## 3. Initialization and Moves
### START size
Initializes an empty square board. The engine does not place a stone yet.
The response is `OK` on success or `ERROR` with an optional explanation on failure.

```text
Manager: START 15
Engine:  OK
```

**moku:** If no readable size is provided, the current default is 15. The handler
accepts values from 5 to 20, but the board representation only has capacity for
19x19 squares. Therefore, use **5 through 19 only**. An `OK` response to `START 20`
does not currently mean that moku can safely handle a 20x20 board.
The 20x20 support required by the reference is therefore not satisfied.

### BEGIN
Requests the first move on an empty board. The engine records its move and
responds with its coordinates. For predefined openings, the manager uses
`BOARD` instead.

```text
Manager: BEGIN
Engine:  7,7
```

**moku:** On an empty 15x15 board, the engine plays the center square `7,7`.

### TURN X,Y
The manager reports the opponent's latest move. The engine applies that move,
calculates and records its own move, and responds with `X,Y`.

An engine move already returned through `BEGIN`, `TURN`, or `BOARD` must not be
sent back to the same engine as an opponent's move.

**moku:** Invalid coordinates, out-of-bounds values, and occupied squares produce
an error response, for example:

```text
ERROR invalid coordinates
ERROR coordinates out of bounds
ERROR position already occupied
ERROR no valid move found
```

### BOARD ... DONE
Transfers a complete position, not just the stones added since the previous move.
After `BOARD`, each square is described by a line in the form `X,Y,field`.

| `field` | Meaning according to the specification |
| --- | --- |
| `1` | Own stone from the receiving engine's perspective |
| `2` | Opponent's stone |
| `3` | Marked square, such as part of a winning line in continuous play or a forbidden square under Renju rules |

`1` and `2` are **not fixed identifiers for black and white**. When sending the
same position to the other engine, these values must be swapped. For Renju,
the specification requires the original move order; for Gomoku, the order
may differ.

`DONE` completes the position and immediately requests the next engine move.
It is not a standalone move command.

```text
BOARD
7,7,1
8,7,2
DONE
```

The engine then responds with a move in `X,Y` format. According to the reference,
the manager sends `RESTART` first if a game is already in progress.

**moku:** The existing board is discarded when `BOARD` is received. Only field
values `1` and `2` are applied; `3` is not implemented. Black and white are
assigned based on stone counts, which is not correct for every combination
of engine color and position. In particular, positions in which the opponent
has one more stone should not be considered reliably supported.
Incomplete or invalid blocks are not comprehensively validated.

## 4. Settings with INFO
Syntax: `INFO key value`. There is **no response on stdout**. Unknown keys may
be ignored. According to the specification, problems with limits should also
be reported only when the next command requiring a response is received.

| Key | Meaning according to the specification | Handling in moku |
| --- | --- | --- |
| `timeout_turn` | Time limit per move in milliseconds; `0` means play as fast as possible | Used for the search time budget; note the deviation for `0` |
| `timeout_match` | Total game time limit in milliseconds; `0` means unlimited | Stored but not used for time management |
| `time_left` | Remaining game time in milliseconds | Stored but not used for time management |
| `max_memory` | Memory limit in bytes; `0` means unlimited | Stored but not enforced as a memory limit |
| `game_type` | `0`: human, `1`: engine, `2`: tournament, `3`: network tournament | Stored |
| `rule` | Bitmask for rule variants | Stored; does not change the game rules |
| `evaluate` | Mouse position `X,Y` for optional debug evaluation | Ignored |
| `folder` | Directory for persistent data | Stored; currently read only up to the next whitespace character |
| `depth` | moku extension, not part of the linked specification | Sets the maximum search depth to a value from 1 to 20 |

The specification's `rule` bits can be combined:

- `1`: Exactly five stones in a row win.
- `2`: Continuous play.
- `4`: Renju.
- `8`: Caro.

Storing a `rule` value in moku does not mean that the corresponding variant
is implemented.

When game time is limited, the manager sends `time_left` before each move
request. Negative values are possible; `2147483647` represents unlimited
game time. According to the reference, time per move also includes command
processing, except for initialization.

```text
INFO timeout_turn 10000
INFO timeout_match 300000
INFO time_left 290000
INFO depth 8
```

**moku time management:** The default limit is 20000 ms per move.
For a positive move limit, the search budget is calculated as
`max(100, timeout_turn - 500)` milliseconds. For a value of `0` or less,
5000 ms is used instead. For `0`, this does not match the protocol requirement
to play as quickly as possible. Very small positive limits may also be
exceeded because of the minimum budget. The manager must therefore monitor
total game time and memory limits externally.

## 5. Management and Optional Commands
### END
Shuts down the engine without any further protocol response. According to the
specification, this should happen immediately.

**moku:** The command loop ends on `END` or when the input stream is closed.
Because search runs synchronously during command processing, an `END` received
during a search is not read until the search finishes. The same applies to
`INFO` updates.

### ABOUT
Responds with one line of comma-separated entries in `key="value"` format.
moku currently responds with:

```text
name="moku", version="0.0.7"
```

The version comes from
[GameConfig](../moku/config.hpp), not from the CMake project version.

### RESTART
Clears the board without changing its size and responds with `OK`.
moku preserves the global settings. The manager can then send, for example,
`BEGIN` or a new `BOARD` block.

### TAKEBACK X,Y
Removes the specified stone and responds with `OK`. For an empty square, moku
reports `ERROR no stone at position`. The command does not generate a new move.
To undo a full pair of moves, both stones must be removed.

**Limitation:** moku does not fully validate coordinate bounds here.
Only send valid coordinates within the current board.

### RECTSTART width,height
The specification uses this command for rectangular boards. moku responds with:

```text
ERROR rectangular board not supported
```

### PLAY and SWAP2BOARD
According to the specification, `PLAY X,Y` confirms or changes a move previously
proposed with `SUGGEST`. moku does not use this workflow and responds to `PLAY`
with `UNKNOWN PLAY`.

`SWAP2BOARD` handles the Swap2 opening. Depending on the phase, zero, three, or
five coordinate lines are sent, followed by `DONE`. The response may contain
three initial stones, one move, two additional stones, or `SWAP`. Once the
opening is complete, the position is transferred using `BOARD` for normal play.

moku does not implement this data block and responds with `UNKNOWN SWAP2BOARD`.
The manager should not send the block: subsequent coordinate lines would be
processed as individual unknown commands.

## 6. Engine Responses
| Response | Meaning |
| --- | --- |
| `X,Y` | Move played by the engine |
| `OK` | Management command completed successfully |
| `UNKNOWN ...` | Command is unknown or not implemented |
| `ERROR ...` | A known command cannot be executed |
| `MESSAGE ...` | Optional single-line information for users |
| `DEBUG ...` | Optional single-line debug information |
| `SUGGEST X,Y` | Move proposal without immediately changing the internal board; the manager responds with `PLAY` or `END` |

The last three response forms are part of the specification but are not
currently produced by moku's standard Gomocup handler. Local diagnostics,
such as those following `INFO depth`, go to stderr instead.

## 7. Example Session with moku
The `Manager:` and `Engine:` prefixes are explanatory only and are **not**
transmitted. This session uses the supported 15x15 board:

```text
Manager: START 15
Engine:  OK
Manager: INFO timeout_turn 10000
Manager: INFO depth 8
Manager: ABOUT
Engine:  name="moku", version="0.0.7"
Manager: BEGIN
Engine:  7,7
Manager: TAKEBACK 7,7
Engine:  OK
Manager: RESTART
Engine:  OK
Manager: END
```

There is no stdout response after `INFO` or `END`. During a game, each opponent
move is followed by `TURN X,Y`; the engine's response depends on the position,
search depth, and time budget.

## 8. Integration with a Game Manager
- For automatic detection of the pipe protocol, the reference requires an
  executable filename beginning with `pbrain-`. moku's build output is named
  `gomocup_moku.exe`, so managers using this convention may require a copy named
  `pbrain-moku.exe`.
- The working directory is set by the manager and does not have to match the
  executable's directory.
- According to the reference, persistent data may only be stored after
  `INFO folder`, in a dedicated subdirectory named after the engine.
- After sending a command that requires a response, a manager must not assume
  that the engine needs further input before responding: with synchronous
  engines, this can cause both sides to wait for each other.
- `extended_moku` additionally supports `DISPLAY` or `D`, `HELP`, and
  `BENCHMARK depth time_limit_ms`. These debug commands are **not part of the
  Gomocup protocol**. Use `gomocup_moku` for tournament communication.

## 9. Implementation Locations
- [Gomocup handler](../moku/engine/gomocup_protocol/gomocup_engine.cpp):
  command processing and protocol responses.
- [Extended handler](../moku/engine/extended_protocol/extended_protocol.cpp):
  additional debug and benchmark commands.
- [Configuration](../moku/config.hpp): defaults and `INFO` settings.
- [Board representation](../moku/game/board.hpp): coordinates and maximum board capacity.
- [Protocol tests](../moku/tests/gomocup_protocol_test.cpp): tests for the Gomocup handler.