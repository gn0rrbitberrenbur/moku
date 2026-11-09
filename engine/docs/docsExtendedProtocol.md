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
| `DEBUG_OUTPUT [interval_ms]` | Toggle stderr diagnostics and JSON search metrics | `DEBUG_OUTPUT True` or `DEBUG_OUTPUT False` |

Command words are case-insensitive. Standard commands and their current
limitations are described in [docsGomocupProtocol.md](docsGomocupProtocol.md).

## 2. INFO max_nodes
`INFO max_nodes <count>` sets the maximum number of recursive minimax nodes
searched by each subsequent search. The value must be a nonnegative integer;
`0` disables the limit. The limit is shared by all SMP workers, so the total
node count does not exceed the configured value. Root move generation and root
candidate evaluation are not included in this count. If the limit interrupts a
search, the engine returns its best available legal move.

Example:

```text
INFO max_nodes 1000000
```

An invalid or missing value leaves the current limit unchanged. The setting is
read when a search starts and applies to searches in the extended engine,
including `BENCHMARK`.

## INFO max_memory
`INFO max_memory <bytes>` bounds the extended engine's TT entry storage. The value
must be one nonnegative integer representable by `long long`. `0` restores the
default `2^23` slots. A positive value must fit at least one `TTEntry`; the table
uses the largest power-of-two slot count that fits the budget, capped at `2^23`.
Larger budgets retain the default maximum rather than growing beyond it.

```text
INFO max_memory 67108864
```

This example caps TT entry storage at 64 MiB. The setting applies immediately
to the current agent and to fresh agents used by subsequent `BENCHMARK` commands.
All search paths, including SMP, use the same agent-local table; workers do not
allocate separate tables. `START` and `RESTART` retain the selected capacity.

A capacity change clears cached entries and occupancy while preserving the board
and Zobrist keys. A budget that selects the current capacity retains cached entries.
Invalid or missing values and allocation failures leave the previous setting and
table unchanged. Success and error diagnostics go to stderr; no response is written
to stdout. `HELP` lists the option.

The budget covers entry-vector allocation, not locks, Zobrist keys, other engine
objects, or allocator overhead. During resizing, old and replacement vectors coexist
temporarily; this is not a process-wide or peak-memory limit. During `BENCHMARK`,
the protocol agent's table remains allocated alongside one workload agent's table,
each subject to the selected budget.

`gomocup_moku` stores `max_memory` as metadata without resizing its table.
Programmatic agents use the default capacity unless an explicit TT budget is supplied.
Debug metrics report the actual selected capacity and allocated bytes; see
[docsTranspositionTable.md](docsTranspositionTable.md) and [docsSearch.md](docsSearch.md).

## 3. DISPLAY and D
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

## 4. HELP
Prints `MESSAGE` lines describing `START`, `BEGIN`, `TURN`, `BOARD`, `INFO`,
`ABOUT`, `END`, `DEBUG_OUTPUT`, and the extended `INFO max_memory` setting.

Use this document for the full extension reference rather than assuming that
every implemented command appears in the help output.

## 5. BENCHMARK depth time_limit_ms
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

## 6. DEBUG_OUTPUT
Debug output defaults to disabled. `DEBUG_OUTPUT [interval_ms]` inverts
`g_config.debug_output` on each call. An optional positive integer sets the
progress interval in milliseconds; omitting it retains the current interval
(default 10 ms). Invalid arguments produce an `ERROR` reply without toggling.
It writes the resulting state to stdout:

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

Search metrics use `[METRICS] ` followed by JSON on stderr. Periodic
`search_progress` and final `search_finished` events include TT capacity, allocated
bytes, occupancy, probes, hits, completed depth, active workers, alpha-beta
cutoffs, and root candidate count. Final events bypass interval throttling.
See [docsSearch.md](docsSearch.md) for field definitions. The Python
[match runner](../../benchmarking/README.md) logs both the parsed object and raw
output. This extension is not part of the standard Gomocup protocol.

## 7. Example Session and Limitations
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

## 7. Implementation Locations
- [Extended entry point](../moku/main_extended_protocol.cpp).
- [extended_protocol.hpp](../moku/engine/extended_protocol/extended_protocol.hpp):
  handler interface.
- [extended_protocol.cpp](../moku/engine/extended_protocol/extended_protocol.cpp):
  dispatch and extended command bodies.
- [Benchmark implementation](../moku/benchmark/benchmark.cpp): workload execution.