# Engine Module Reference
The engine layer connects stdin commands to board mutation and move selection.
It provides two separate implementations of `Engine`: One for the Gomocup executable and one for the extended executable.

## 1. Entry Points and Ownership
| Executable | Entry point | Handler |
| --- | --- | --- |
| `gomocup_moku` | `main_gomocup.cpp` | Gomocup `Engine` |
| `extended_moku` | `main_extended_protocol.cpp` | Extended `Engine` |

Both entry points construct an engine and call `run()`. They do not parse
command-line arguments or create an interactive application.

Each engine owns:
- A `Board` containing the current stones.
- A `MinimaxAgent` initialized from `g_config.max_depth`.
- A `running` flag, initially `true`.

The handlers define classes with the same name and overlapping method names.
They are alternatives compiled into different targets, not interchangeable
objects intended to be linked together in one executable.

## 2. Command Loop
`run()` reads lines from `std::cin` until the input ends or `END` clears the
running flag. Nonempty lines are passed to `process_command()`, followed by
an stdout flush. The dispatcher uppercases the command word.

Processing is synchronous. A search blocks further command reads, even when
the search itself uses worker threads. `END` and `INFO` therefore do not
interrupt an active search through a separate input thread.

`BOARD` is special: its handler reads subsequent lines directly from stdin
until `DONE` or end of input.

## 3. Move and State Flow
| Command | Main implementation behavior |
| --- | --- |
| `START` | Changes the global size and replaces the board |
| `BEGIN` | Plays the center on an empty board; otherwise uses timed search as black |
| `TURN` | Validates a reported opponent move, infers its color from counts, then uses timed SMP search |
| `BOARD` | Replaces the board from a multiline position and uses timed single-threaded search |
| `RESTART` | Replaces the board without resetting global settings |
| `TAKEBACK` | Clears the specified occupied color bit |
| `INFO` | Updates recognized configuration fields |
| `ABOUT` | Prints engine name and configured version |

The handler applies the engine's selected move before returning its coordinates.
The search itself does not commit the move.

There is no stored engine-color field or move-history list. The `TURN` path
uses equal black/white counts to infer that the opponent plays black next.
The `BOARD` path maps own/opponent fields using their counts. These assumptions
do not represent a complete game-state validation system.

## 4. Output and Error Handling
Protocol responses go to stdout and are flushed. Conditional diagnostics go
to stderr. Invalid `TURN` syntax, out-of-range coordinates, occupied squares,
and missing search moves have explicit error responses. Unknown commands
return `UNKNOWN` without ending the loop.

Validation is not uniform: `BOARD` ignores some malformed data, `TAKEBACK`
does not fully check bounds, and `START 20` exceeds fixed board capacity.
The agent is not reconstructed when `START` changes size, which matters for
table keys initialized at construction.

For exact command syntax and compatibility warnings, see
[the Gomocup protocol reference](docsGomocupProtocol.md). Extended commands
are documented in [the extended protocol reference](docsExtendedProtocol.md).

## 5. Usage Example
After starting the appropriate executable, send:

```text
START 15
INFO depth 6
BEGIN
END
```

The engine responds `OK`, then the center coordinate `7,7`. `INFO` and `END`
produce no stdout response. Commands are input lines, not executable flags.

## 6. Implementation Locations
- [Gomocup entry point](../moku/main_gomocup.cpp).
- [Extended entry point](../moku/main_extended_protocol.cpp).
- [Gomocup interface](../moku/engine/gomocup_protocol/gomocup_engine.hpp).
- [Gomocup implementation](../moku/engine/gomocup_protocol/gomocup_engine.cpp).
- [Extended interface](../moku/engine/extended_protocol/extended_protocol.hpp).
- [Extended implementation](../moku/engine/extended_protocol/extended_protocol.cpp).