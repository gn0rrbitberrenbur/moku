# moku
moku is a C++17 Gomoku engine with two protocol-driven executables: 
- A Gomocup protocol compliant engine
- An extended build with debugging and benchmark commands. 

## Requirements
- CMake 3.14 or newer
- A C++17 compiler
- Network access during initial configuration to fetch GoogleTest

## Build
Run these commands from the repository root to build both engines and the test targets:

```bash
cmake -S moku -B moku/build -DCMAKE_BUILD_TYPE=Release
cmake --build moku/build --config Release
```

Alternatively, run the build helper from the repository root:

```powershell
.\scripts\build.ps1
```

```bash
bash scripts/build.sh
```

The CMake targets are 
- `gomocup_moku`
- `extended_moku`
- `moku_core`
- `tests`
- `gomocup_protocol_tests`

## Tests
Run the registered GoogleTest suites from the repository root:

```bash
ctest --test-dir moku/build -C Release --output-on-failure
```

To only run the Gomocup Protocol test suite run from the repository root:
```bash
ctest --test-dir moku\build -C Release --output-on-failure -R "^GomocupProtocolTest\."
```

To only run the normal test suite run from the repository root:
```bash
ctest --test-dir moku\build -C Release --output-on-failure -R "^(AlgebraicConversion|BoardTest|EvaluatorTest|EngineTest|TranspositionTableTest)\."
```

## Protocols
Both executables read commands from standard input and write responses to standard output. 
The Gomocup command set includes:

| Command | Parameters | Description |
| --- | --- | --- |
| `START` | `size` (5-19; optional, default: 15) | Initializes an empty square board. |
| `BEGIN` | none | Requests the first move; on an empty board, the engine plays the center. |
| `TURN` | `X,Y` | Reports the opponent's move and requests an engine move. |
| `BOARD` | Data block of `X,Y,field` lines, terminated by `DONE` | Loads a complete position and requests the next move. `field` is `1` for an own stone and `2` for an opponent's stone. |
| `INFO` | `key value`, e.g. `timeout_turn 10000` or `depth 8` | Sets game and search parameters. Supported keys are `timeout_turn`, `timeout_match`, `time_left`, `max_memory`, `game_type`, `rule`, `folder`, and `depth` (1-20); some values are stored but not used. |
| `ABOUT` | none | Returns engine information. |
| `RESTART` | none | Resets the board while keeping its size. |
| `TAKEBACK` | `X,Y` | Removes the stone at the specified coordinates. |
| `END` | none | Stops the engine. |

`extended_moku` supports these commands and adds:

| Command | Parameters | Description |
| --- | --- | --- |
| `DISPLAY` or `D` | none | Displays the current board and stone counts. |
| `HELP` | none | Prints a short command summary. |
| `BENCHMARK` | `depth time_limit_ms` (`depth` > 0, `time_limit_ms` >= 0) | Runs the predefined benchmark positions; the time limit applies to each position. |

Coordinates use the `X,Y` format. Command names are case-insensitive.

Debug output is disabled by default. The former `--debug-output` startup flag has been
removed. A `DEBUG_OUTPUT` handler toggles diagnostics and replies with `DEBUG_OUTPUT True`
or `DEBUG_OUTPUT False`, but it is not yet registered in the command dispatcher. Sending
`DEBUG_OUTPUT` currently returns `UNKNOWN DEBUG_OUTPUT`, despite its entry in `HELP`.
See the [extended protocol reference](docs/docsExtendedProtocol.md#5-debug_output) for details.

An empty board starts at its center. The default board size is 15, search depth is 12, and the
default turn time limit is 20 seconds. Settings such as turn time and search depth can be sent
with Gomocup `INFO` commands.

## Documentation
See the [documentation overview](docs/docsOverview.md) for reference.