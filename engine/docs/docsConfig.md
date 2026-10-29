# Configuration Reference
[config.hpp](../moku/config.hpp) defines `GameConfig`, the shared configuration
structure used by moku's board, protocol handlers, and search code. It contains
default values and runtime settings.
Neither executable provides command-line flags for these settings.

## 1. Structure and Global Instance
The header declares the configuration type and a single shared instance:

```cpp
struct GameConfig
{
    // Settings and helper function.
};

inline GameConfig g_config;
```

The C++17 inline variable allows the header to be included in multiple translation
units while referring to the same `g_config` instance. Its fields are public and
mutable. The structure itself does not validate assignments or synchronize access.

Changing a default initializer in the header requires rebuilding the executables.
Supported protocol commands can update selected settings at runtime.

## 2. Fields and Defaults
| Field | C++ type | Default | Purpose |
| --- | --- | --- | --- |
| `version` | `std::string` | `"0.0.7"` | Engine version returned by `ABOUT` |
| `board_size` | `int` | `15` | Side length of the square board |
| `max_depth` | `int` | `12` | Initial maximum search depth for protocol engines |
| `time_limit_ms` | `int` | `20000` | Per-move time setting in milliseconds |
| `timeout_match` | `int` | `0` | Stored total game time limit in milliseconds |
| `time_left` | `int` | `2147483647` | Stored remaining game time in milliseconds |
| `max_memory` | `long long` | `0` | Stored memory limit in bytes |
| `game_type` | `int` | `0` | Stored opponent or game category |
| `rule` | `int` | `0` | Stored rule bitmask |
| `folder` | `std::string` | Empty string | Stored directory for persistent data |
| `debug_output` | `bool` | `false` | Enables conditional search and protocol diagnostics |
| `cores` | `unsigned int` | Hardware concurrency, or `1` if unavailable | Thread count requested by the Lazy SMP search |

Some fields only store information received from the manager. Their presence does
not imply that the corresponding limit or feature is enforced.

## 3. Board Settings
### board_size
Controls the board dimensions and row-major coordinate conversion:

```text
position = row * board_size + column
```

For Gomocup coordinates, `column` is `X` and `row` is `Y`.

Use `START size` to change the board size through the protocol. The handler updates
`g_config.board_size` and creates a new empty board. Do not change this field directly
while a position is in use: existing bit positions would be interpreted using the
new dimensions.

### squares()
Returns the number of squares implied by the configured side length:

```cpp
int squares() const
{
    return board_size * board_size;
}
```

For the default size of 15, the result is `225`. This helper performs no range
validation and does not report the fixed storage capacity.

## 4. Search Settings
### max_depth
The protocol engine constructs its `MinimaxAgent` using `g_config.max_depth`.
The agent then stores its own depth limit, so assigning a different value to
`g_config.max_depth` alone does not update an existing agent.

Use the moku-specific command `INFO depth N` to update both the global value and
the protocol engine's agent. Accepted values are **1 through 20**. This command
is an extension, not part of the standard Gomocup protocol.

The depth is an upper bound, not a guarantee that a timed search completes that
many plies. Iterative deepening may stop earlier when its time budget expires.

### time_limit_ms
Updated by `INFO timeout_turn N`. For searches requested through `BEGIN`, `TURN`,
or `BOARD`, the handlers derive a search budget as follows:

```text
If time_limit_ms > 0:
    search budget = max(100, time_limit_ms - 500) milliseconds
Otherwise:
    search budget = 5000 milliseconds
```

The default setting therefore produces a search budget of `19500` ms when search
is needed. On an empty board, `BEGIN` plays the center without searching.

The 500 ms subtraction provides a margin, but the 100 ms minimum can exceed a
very small requested limit. Also, the Gomocup meaning of `timeout_turn = 0`
is to play as quickly as possible, moku currently uses a 5000 ms budget instead.
The setting is not a strict end-to-end response deadline.

### cores
Initialized using `std::thread::hardware_concurrency()`, with a fallback of one
when the reported value is zero. The reported hardware concurrency is a hint,
not a guarantee of available CPU resources.

`get_best_move_timed_smp()` reads this value to select the requested thread count.
A value of `0` or `1` falls back to the single-threaded timed search. The protocol
uses the SMP entry point for `TURN`; searches from `BEGIN` and `BOARD` use the
single-threaded timed entry point.

There is no protocol command for changing `cores`. Set it programmatically before
starting a search, or change its default and rebuild. Avoid modifying the shared
configuration while worker threads are running.

## 5. Stored Gomocup Information
The following settings are accepted by both protocol handlers but are not
currently used to enforce limits or select game behavior:

| Field | Runtime command | Meaning |
| --- | --- | --- |
| `timeout_match` | `INFO timeout_match N` | Total game time limit in milliseconds; `0` means unlimited |
| `time_left` | `INFO time_left N` | Remaining game time; `2147483647` means unlimited |
| `max_memory` | `INFO max_memory N` | Memory limit in bytes; `0` means unlimited |
| `game_type` | `INFO game_type N` | `0`: human, `1`: engine, `2`: tournament, `3`: network tournament |
| `rule` | `INFO rule N` | Rule-variant bitmask |
| `folder` | `INFO folder path` | Directory supplied for persistent data |

Negative `time_left` values are possible under the protocol when the engine has
run out of game time. moku stores them but does not adjust its search budget
based on them. Total game time and memory limits must be monitored externally.

The specification defines the following combinable `rule` bits:

- `1`: exactly five stones in a row win.
- `2`: continuous play.
- `4`: Renju.
- `8`: Caro.

Assigning these bits does **not** switch moku to the corresponding rules.

`folder` defaults to an empty string. The current parser reads one
whitespace-delimited token, so paths containing spaces are not read in full.
Storing the path does not itself create a directory or write persistent files.

## 6. Version and Diagnostics
### version
Used by `ABOUT` to produce:

```text
name="moku", version="0.0.7"
```

This value is independent of the version declared in
[CMakeLists.txt](../moku/CMakeLists.txt). Do not assume that the two versions
are synchronized.

### debug_output
Enables conditional diagnostics in search code and protocol move handling.
These diagnostics are written to stderr to preserve protocol output on stdout.
There is no `INFO` command for this flag, and the former `--debug-output` startup
flag has been removed.

The extended engine's `cmd_debug_output()` handler toggles this value and replies
on stdout with `DEBUG_OUTPUT True` or `DEBUG_OUTPUT False`. It is listed by
`HELP`, but is not yet registered in `process_command()`: sending `DEBUG_OUTPUT`
currently returns `UNKNOWN DEBUG_OUTPUT`. See the
[extended protocol reference](docsExtendedProtocol.md#5-debug_output).

This is not a universal logging switch: for example, a valid `INFO depth`
command writes its depth-change message to stderr regardless of this flag.
The benchmark implementation also explicitly sets `debug_output` to `false`.

## 7. Runtime Configuration Example
Send these lines to either protocol executable through stdin:

```text
START 15
INFO timeout_turn 10000
INFO depth 8
INFO timeout_match 300000
INFO time_left 290000
INFO max_memory 83886080
```

`START` responds with `OK`; the `INFO` commands produce no stdout response.
The active search depth becomes 8, and the derived search budget is 9500 ms.
The total game time, remaining time, and memory values are stored but not enforced.

`RESTART` clears the board without resetting the global configuration. `START`
updates the board size and clears the board, but does not restore the other fields
to their default values. Restarting the process constructs a fresh global instance.

For the complete command reference, see the
[Gomocup protocol documentation](docsGomocupProtocol.md).

## 8. Implementation Locations
- [config.hpp](../moku/config.hpp): configuration fields, defaults, and global instance.
- [Board](../moku/game/board.hpp): board dimensions and fixed storage capacity.
- [Gomocup engine](../moku/engine/gomocup_protocol/gomocup_engine.hpp):
  agent construction using the configured depth.
- [Gomocup handler](../moku/engine/gomocup_protocol/gomocup_engine.cpp):
  runtime updates and search-budget calculation.
- [Extended handler](../moku/engine/extended_protocol/extended_protocol.cpp):
  matching configuration handling for the extended executable.
- [MinimaxAgent](../moku/search/minimax.hpp): agent-local search depth and setter.
- [Search implementation](../moku/search/minimax.cpp): timed search and Lazy SMP.
- [Benchmark implementation](../moku/benchmark/benchmark.cpp): benchmark diagnostic settings.