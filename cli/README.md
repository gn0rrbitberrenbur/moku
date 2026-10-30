# Moku CLI
A C++17 terminal application for playing Gomoku against external engines or watching
two engines compete.

---
> **Warning!!!** <br>
> Do **not** run this application with administrator privileges. <br>
> The used process isolation is **not** a security sandbox. <br>
> **Every engine** that you will load in **does not run sandboxed** and with the **same privileges as the source process**. <br>
> Therefore, if you run the cli with administrator privileges, the **engine applications** you load in will run with **administrator privileges** too.
---

## Requirements
- CMake 3.15 or newer
- A C++17 compiler

## Quick start
### 1. Add engine
Open **Options -> Add engine**.

Enter a display name and the path to an executable.

### 2. Choose Gamemode
Select **Play vs engine** or **Engine vs engine**.

### 3. Game setup
Configure the board, rules and turn time.

Select each engine from the arrow-key list.

### 4. Start game
Choose **Start game**.

### 5. Gameloop
On a human turn, move the highlighted square with the arrow keys and press Enter

Alternatively type a coordinate (`H8`) and press Enter.

Arrow navigation clears a partly typed coordinate.

### 6. Escape game
Press Esc to leave a game and stop its engine processes.

### Engine vs. Engine
In engine vs engine, Space pauses/resumes the match **between moves**. 

An already-running search continues with its original deadline, its result is held until resume. 
There is no artificial delay between engine turns other than the UI refresh interval.

### Test mode
**Test mode** plays a series of games between engine A and engine B without a live board. 

The setup screen sets the number of games, board size, rule, time per move, random opening stones (0-8), seed, both engines and the result file.

Engines alternate Black, and each random opening is played twice with swapped colors.

A progress screen shows the running score.

Esc aborts and keeps the finished games.

The final report and the JSON file contain:
- Wins/losses/draws 
- Score percent
- Results as Black and White
- Forfeits by cause (Timeout, illegal move, crash, protocol)
- Move time average/median/maximum
- Game length
- Elo difference with a 95% confidence interval
- Every gamess full move list

The default file is saved as `results/test-YYYYMMDD-HHMMSS.json` in the working directory.

It is rewritten after each game.

### Debug mode
Select **Debug > Play vs engine** or **Debug > Engine vs engine**, then configure the game
as usual. 

The CLI probes each engine with the optional `DEBUG_OUTPUT` toggle after startup.

An engine acknowledging `DEBUG_OUTPUT True` can report `[METRICS]` JSON on stderr.

Every debug game creates a separate `debug-reports/debug-<timestamp>.json` in the current
working directory. It records commands and complete stdout/stderr from both engine instances, including startup and shutdown, with timestamps and engine/color identifiers.

The file is flushed as events arrive, including when a game is cancelled or an engine fails.

On leaving the game, the CLI displays the saved path. Write failures are reported explicitly.

See [docsEngine.md](docs/docsEngine.md#debug-metrics-and-transcripts) for the JSON format.

## Persistence
Engine entries are stored in a versioned, quoted UTF-8 text file.

Names, spaces, quotes and backslashes round-trip correctly.

- Windows: `%APPDATA%\moku-cli\engines.cfg`
- Linux: `$XDG_CONFIG_HOME/moku-cli/engines.cfg`, or `$HOME/.config/moku-cli/engines.cfg`

> Override this by running: `moku_cli --config <path>`

> Only run **trusted executables**: engine subprocesses have the same permissions as the CLI. <br>
> Process isolation is not a security sandbox.

## Engine protocol
Engines must support the
[Gomocup pipe protocol](https://plastovicka.github.io/protocl2en.htm).

The executable name does not need the `pbrain-` prefix for this manager.