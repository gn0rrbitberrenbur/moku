# CLI engine sessions
## Purpose
Run trusted Gomocup pipe-protocol engines outside the UI process and communicate without blocking
terminal input.

## Files
- [engine.hpp](../moku-cli/engine/engine.hpp), [engine.cpp](../moku-cli/engine/engine.cpp): sessions and workers.
- [process.hpp](../moku-cli/process/process.hpp), [process.cpp](../moku-cli/process/process.cpp): native subprocess transport.
- [debug.hpp](../moku-cli/engine/debug.hpp), [debug.cpp](../moku-cli/engine/debug.cpp):
  thread-safe live metrics and streamed JSON debug transcripts.
- [json.hpp](../moku-cli/file_handling/json.hpp), [json.cpp](../moku-cli/file_handling/json.cpp):
  dependency-free JSON parsing and escaping shared with test reports.

## Interface
`EngineWorker(entry, config)` validates the executable and creates one dedicated `std::thread`.
`ready()` supplies the startup future; `request(optionalOpponentMove)` supplies a move future.
`requestBoard(position, own)` imposes a complete position with `BOARD` and requests a move.
Move futures carry an `EngineReply` with the move and the milliseconds from sending the request
to the parsed reply. `stop()` cancels current/queued work and wakes idle workers. Destruction
joins the worker and releases the owned process. Startup and protocol failures propagate through
futures as `EngineError` ([engine_error.hpp](../moku-cli/engine/engine_error.hpp)) with a failure kind:
`Timeout`, `IllegalMove` (coordinates outside the board), `Crash` (exit, EOF or pipe failure),
`Protocol` (`ERROR`, `UNKNOWN`, malformed or oversized replies) or `Cancelled`.

Each worker launches exactly one executable, without a shell or extra command-line arguments.
Its working directory is the executable's parent directory. Paths with spaces are supported.
Separate pipes carry stdin, stdout and stderr. Stderr is drained during responses and its last
4096 bytes are attached to protocol failure diagnostics, rather than corrupting the board display.
While waiting for a reply, Windows polls the stdout pipe about every millisecond using a
high-resolution waitable timer (anonymous pipes cannot be waited on); Linux uses `poll`.
Transport latency is therefore about 1-2 milliseconds per move instead of the default timer tick.

## Protocol
1. Send `START <size>` and require `OK` or `OK <description>` within 10 seconds.
2. Send `INFO timeout_turn`, `timeout_match 0`, `max_memory 0`, `game_type` and `rule`.
3. Before each request send `INFO time_left 2147483647`.
4. Send `BEGIN` on an empty board, otherwise `TURN X,Y` for the preceding opponent move.
   Test mode openings use `BOARD`, one `X,Y,field` line per stone in play order (1 = own,
   2 = opponent) and `DONE`.
5. Parse one zero-based coordinate and let the application validate occupancy and game outcome.
6. Send `END` during worker shutdown, close stdin and terminate processes that do not exit.

Commands end in CRLF; stdout accepts LF, CRLF and CR. Empty, `DEBUG` and `MESSAGE` lines are
ignored. `ERROR`, `UNKNOWN`, unexpected replies, oversized lines (over 8192 bytes), EOF and
timeouts produce explicit failures. `RESTART` is never sent because the manager starts new
processes for each game and never rewinds the board.

Turn deadlines use `steady_clock`, include transport/parsing and have no grace period.
The opponent wins by forfeit after a move failure. A startup failure cancels the setup instead.
CPU scheduling, search parallelism and optional INFO support remain the engine's responsibility.

## Debug metrics and transcripts
Only debug games set `EngineConfig.debug` to a shared `DebugSession`, with `debugEngine` equal
to the player's color index (0 = Black, 1 = White, including human games).
After START and INFO, each worker sends `DEBUG_OUTPUT` with no arguments, never a startup flag.
The optional acknowledgement has a maximum 200 ms startup wait, bounded by the existing startup
deadline. `DEBUG_OUTPUT True` enables parsing. If an engine reports False because debug was
already enabled on launch, the worker toggles it once more to enable it.
ERROR/UNKNOWN or no acknowledgement disables metrics without failing startup. Late toggle
acknowledgements and ERROR/UNKNOWN replies naming `DEBUG_OUTPUT` do not contaminate move replies.
Late True acknowledgements also enable metrics for subsequent progress events.
Move deadlines are unchanged.

The transport observer records all sent stdin and received stdout/stderr chunks, before stderr
is truncated for error diagnostics. Debug workers also poll idle processes and drain both pipes
during bounded shutdown. The worker reassembles stderr lines across pipe reads; complete lines
beginning `[METRICS] ` are parsed as JSON. Supported events have this shape:

```json
{
  "event": "search_progress",
  "elapsed_ms": 150,
  "nodes_total": 1000,
  "tt_hits_total": 100,
  "nps": 6666,
  "depth_current": 2,
  "best_move": {"x": 0, "y": 0},
  "score": -42
}
```

Field order and extra fields are allowed. Counters are nonnegative 64-bit integers. `best_move`
may be null or a zero-based coordinate within the configured board; `score` may be a finite JSON
number or null. Invalid JSON, missing/wrongly typed fields and metrics lines exceeding 8192 bytes
produce a `metrics_error` transcript entry and clear the live snapshot, not a game forfeit.
Normal stderr remains diagnostic text. The UI reads mutex-protected snapshots and never accesses
the engine process directly.

The automatically named report in `debug-reports` is a single JSON document:

```json
{
  "started_at": "2026-10-28T10:00:00Z",
  "events": [
    {
      "timestamp": "2026-10-28T10:00:00Z",
      "elapsed_ms": 12,
      "engine": 0,
      "name": "Moku",
      "stream": "stdin",
      "text": "DEBUG_OUTPUT\r\n"
    }
  ]
}
```

`elapsed_ms` is the monotonic time since transcript creation; `timestamp` is UTC wall time.
Streams are `stdin`, `stdout`, `stderr`, plus `debug_status` for unsupported-command notices and
`metrics_error` for parsing failures. Transport events are chunks, not necessarily whole lines;
concatenate `text` in order for the same engine and stream to reconstruct its output.
Control characters are JSON-escaped; invalid UTF-8 bytes are replaced with U+FFFD, as in test
reports. Both instances remain distinguishable even when they use the same executable/name.
The event array is extended and flushed without accumulating the full transcript in memory;
its closing bracket is rewritten after each event. Normal cancellation and engine failures retain
the transcript. Forced termination or a filesystem failure during a write cannot guarantee an
intact final event. Save errors are surfaced to the UI.

## Platform lifecycle
Windows uses `CreateProcessW`, native pipes and a kill-on-close Job Object containing the child
process tree. Creation is serialized to avoid cross-inheriting another worker's temporary pipe
handles. Pipe buffers accommodate the bounded command volume of a maximum-size game.

Linux uses `fork`/`execv`, close-on-exec pipes, nonblocking I/O and a separate process group.
Broken-pipe signals are ignored so that write failures become exceptions. After stdin closes,
the worker allows a short exit interval and kills any remaining process group members.

Workers check cancellation while reading and waiting for commands. An unresponsive engine is
forcibly cleaned up; it cannot keep the application waiting for a full search time limit.
This isolation is not a security sandbox and does not constrain engine memory or CPU usage.

## Validation
Mock tests exercise two synchronized engine sessions, both rule values/game types, debug output,
large stderr output, CR-only lines, rejected startup, oversized replies, crashes, invalid moves,
deadlines and cancellation during startup/search. Test mode runs cover `BOARD` starts and the
failure classification. A real-engine match is available as an
optional integration check.
Debug tests additionally cover split metrics lines, real-time snapshots, 64-bit counters,
null values, malformed metrics, rejected/silent/already-enabled debug commands, both engine
identities, per-search resets, engine failures and complete JSON transcripts including END/shutdown
output.