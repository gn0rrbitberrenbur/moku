# Extended Protocol Reference
`extended_moku` includes the Gomocup command handling plus local display, help,
and benchmark commands. These extensions are intended for debugging and
measurement, not standard tournament communication.

## 1. Command Overview
| Command | Purpose | Output |
| --- | --- | --- |
| `DISPLAY` | Display the current board | Raw board grid and stone counts |
| `D` | Alias for `DISPLAY` | Same as `DISPLAY` |
| `HELP` | Print a short command summary | Lines prefixed with `MESSAGE` |
| `BENCHMARK depth time_limit_ms` | Run predefined benchmark positions | Human-readable benchmark report |

Command words are case-insensitive. Standard commands and their current
limitations are described in [docsGomocupProtocol.md](docsGomocupProtocol.md).

## 2. DISPLAY and D
Displays the active board using:

- Column letters beginning with `A`.
- Row numbers beginning with `1`.
- `X` for black, `O` for white, and `.` for empty squares.

The grid is followed by:

```text
DEBUG Black stones: <count>
DEBUG White stones: <count>
```

The grid itself is raw stdout text, not a `MESSAGE` or `DEBUG` payload.
The command does not generate a move or modify the board.

## 3. HELP
Prints `MESSAGE` lines describing `START`, `BEGIN`, `TURN`, `BOARD`, `INFO`,
`ABOUT`, `END`, and `DEBUG_OUTPUT`.

Use this document for the full extension reference rather than assuming that
every implemented command appears in the help output.

## 4. BENCHMARK depth time_limit_ms
Example:

```text
BENCHMARK 4 1000
```

The handler requires a positive depth and a nonnegative time limit. Invalid
arguments produce:

```text
ERROR invalid arguments for BENCHMARK
```

There is no protocol-handler upper bound of 20 for benchmark depth. With a
positive time limit, each position uses the timed SMP search. With zero, each
position uses fixed-depth search without a requested time limit. The limit
is per position, not for the entire benchmark.

The command runs synchronously and prints a version header, results table,
and totals to stdout. There is no final `OK` or move response. See
[docsBenchmark.md](docsBenchmark.md) for workloads and metric limitations.

The runner sets `g_config.debug_output` to `false` and does not restore it.
It temporarily changes the global board size for each workload and restores
it on normal completion. It uses separate boards and agents rather than
replacing the protocol engine's current position.

## 5. DEBUG_OUTPUT
The former `--debug-output` startup flag has been removed. Debug output defaults
to disabled. The `cmd_debug_output()` handler takes no arguments and inverts
`g_config.debug_output` on each call. It writes the resulting state to stdout:

```text
DEBUG_OUTPUT True
```

or:

```text
DEBUG_OUTPUT False
```

Conditional search and move-handling diagnostics remain on stderr. This flag
does not suppress explicit `DISPLAY`, `HELP`, or benchmark output, and it is not
a universal logging switch. `BENCHMARK` disables it without restoring its value.

**Current limitation:** `process_command()` has no `DEBUG_OUTPUT` branch, so the
handler is unreachable through stdin. Sending `DEBUG_OUTPUT` currently returns
`UNKNOWN DEBUG_OUTPUT`. Registering the handler is required before runtime
toggling can be used. This extension is not part of the standard Gomocup protocol.

## 6. Example Session and Limitations
```text
START 15
BEGIN
DISPLAY
HELP
BENCHMARK 3 500
END
```

This produces mixed protocol and human-readable output. Use `gomocup_moku`
when a manager requires only standard protocol output.

The header declares `cmd_analyze()`, but there is no implementation or
dispatcher branch for `ANALYZE`. Sending it returns `UNKNOWN ANALYZE`.
There are no dedicated extended-protocol tests at present.

## 7. Implementation Locations
- [Extended entry point](../moku/main_extended_protocol.cpp).
- [extended_protocol.hpp](../moku/engine/extended_protocol/extended_protocol.hpp):
  handler interface.
- [extended_protocol.cpp](../moku/engine/extended_protocol/extended_protocol.cpp):
  dispatch and extended command bodies.
- [Benchmark implementation](../moku/benchmark/benchmark.cpp): workload execution.