# CLI test mode
## Purpose

Play a fixed number of games between two registered engines without a live board, then report
results and statistics in the terminal and as a JSON file.

## Files
- [test_runner.hpp](../moku-cli/tests/test_runner.hpp), [test_runner.cpp](../moku-cli/tests/test_runner.cpp) - openings,
  game loop and run control
- [test_report.hpp](../moku-cli/tests/test_report.hpp), [test_report.cpp](../moku-cli/tests/test_report.cpp) - data
  types, statistics, Elo and JSON
- [test_mode.hpp](../moku-cli/tests/test_mode.hpp), [test_mode.cpp](../moku-cli/tests/test_mode.cpp) - setup, progress
  and report screens
- [file_io.hpp](../moku-cli/file_handling/file_io.hpp), [file_io.cpp](../moku-cli/file_handling/file_io.cpp) - atomic file replacement

## Interface
`runTest(settings, poll)` validates `TestSettings`, plays all games and returns a `TestOutcome`.
`poll` receives a `TestProgress` (current game, stones on the board, Black engine index, finished
games) about every 10 milliseconds and once per move; returning `true` aborts the run with
`TestAborted`. `playTestGame` plays one game and `generateOpenings` creates the opening list.
`summarize` aggregates finished games and `reportJson`/`writeTestReport` serialize them.

## Setup
| Setting | Range | Default |
|---|---|---|
| Games | 1-10000 | 10 |
| Board size | 5-19 | 15 |
| Rule | freestyle or exact five | freestyle |
| Engine time per move | 1-600000 ms | 5000 |
| Random opening stones | 0-8 | 2 |
| Seed | 0-4294967295 | random per setup |
| Engine A / B | registered engines, may be identical | none |
| Result file | any file path | `results/test-YYYYMMDD-HHMMSS.json` in the working directory |

Relative result paths are made absolute. An existing file requires confirmation. Settings except
seed, engines and result file are remembered for the session.

## Game schedule
Game `i` (zero-based) uses opening `i / 2`; engine A plays Black in even games and engine B in odd
games. Every opening is therefore played twice with swapped colors, and the engines alternate
starting. Openings are drawn with `std::mt19937` from the 7x7 center window (smaller on 5x5
boards), contain no duplicate squares and alternate Black/White starting with Black. With at most
8 stones no opening can contain a five.

Every game starts two fresh engine processes, each in its own worker thread. An engine's first
request is `BEGIN` on an empty board, `TURN` if exactly one stone exists, and `BOARD` otherwise.
Later requests are `TURN` with the opponent move.

## Results and forfeits
A game ends with five in a row, a full board (draw), or a forfeit. A forfeit loses the game for the
offending engine and is classified as `timeout`, `illegal_move` (outside the board or occupied
square), `crash` (process exit or pipe failure) or `protocol` (`ERROR`, `UNKNOWN`, malformed or
oversized reply). An engine startup failure aborts the run because no game can be scored.

## Statistics
- Wins, losses, draws, score (win 1, draw 0.5) and score percent per engine
- Wins/losses/draws as Black and as White
- Forfeits by cause
- Move time average, median and maximum, measured from sending the request to the parsed reply
- Game length (stones on the final board, including the opening): average, minimum, maximum
- Elo difference of A versus B, `-400 * log10(1 / score - 1)`, with a 95% confidence interval
  from the per-game W/D/L variance; values are `null` when a bound is undefined

## Report file
The JSON file is UTF-8, written atomically after each finished game and at the end, so an aborted
run keeps all finished games. Top-level fields:

- `format` (`moku-cli-test`), `version` (1), `completed`, `abortReason`, `startedAt`, `finishedAt`
  (UTC ISO 8601)
- `settings`: games, boardSize, rule, turnMilliseconds, openingStones, seed
- `engines`: index, name and path for A (0) and B (1)
- `summary`: gamesPlayed, draws, gameLength, elo (perspective of engine 0) and per-engine statistics
- `games`: index, black/white engine indexes, result, winner, end, length, forfeit details,
  opening stones and every engine move with coordinate, engine, color and milliseconds

## Terminal
The progress screen shows game number, progress bar, colors, stones on the board, current score,
the last result and elapsed time. Esc aborts after the current request is cancelled. The final
report page lists all statistics and the result path.

## Validation
`cli_tests` covers statistics, Elo, JSON escaping, opening determinism, color alternation,
`BEGIN`/`TURN`/`BOARD` starts, every forfeit cause, startup aborts and user cancellation.
`cli_terminal_tests` runs a two-game test through the UI and checks the JSON file.