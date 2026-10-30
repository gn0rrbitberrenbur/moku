# CLI terminal and screens
## Purpose
Provide keyboard-driven menus, engine management, game configuration and a responsive board.

## Files
- [terminal.hpp](../moku-cli/ui/terminal.hpp), [terminal.cpp](../moku-cli/ui/terminal.cpp)
- [application.hpp](../moku-cli/ui/application.hpp), [application.cpp](../moku-cli/ui/application.cpp)
- [ui_helpers.hpp](../moku-cli/ui/ui_helpers.hpp), [ui_helpers.cpp](../moku-cli/ui/ui_helpers.cpp): number
  parsing and engine selection shared by the screens
- [main.cpp](../moku-cli/main.cpp)
- Test mode screens: [docsTestMode.md](docsTestMode.md)

## Interface
`Terminal` owns console state using RAII. Windows uses native console input records, Unicode
keyboard events and ANSI output mode. POSIX uses `termios` and polling. The alternate screen
keeps normal shell output intact; the cursor and console modes are restored on ordinary exit
and exception unwinding. Forced termination or fatal OS signals cannot guarantee restoration.

`menu` supports Up/Down, Enter and Esc; long engine lists scroll. `prompt` supports typing,
Backspace, Enter and Esc. `message` shows one text and `page` shows preformatted lines, both until
Enter or Esc. User/engine text is stripped of ASCII control characters before display.
Identical frames are not redrawn. Ctrl+C/Ctrl+D acts as Back/Esc within the current screen;
it does not immediately terminate the entire application.

## Screens
The title screen offers Play vs engine, Engine vs engine, Test mode, Options, Debug and Quit.
Options supports adding, editing and deleting engines, with confirmation before deletion.
Every new game requires explicit engine selection and offers size/rule/time/color settings.

The board shows column letters, row numbers, player names, move count and current status:

- Arrow keys move the cyan cursor; Enter plays the selected empty square.
- Typing a coordinate then Enter plays that square instead.
- Arrow keys discard partially typed coordinates; Backspace edits them.
- Blue marks the last move unless it is also the cursor.
- Esc leaves the game and cancels both workers.
- In engine vs engine, Space pauses between moves; ongoing searches and deadlines are not frozen.
- Enter returns to the title screen after a win, draw or forfeit.

Rendering and future polling run on the UI thread, with keyboard waits of at most 40 milliseconds
during a game. Before launching engines, the application waits for a terminal of at least
65 columns by `board size + 13` rows. If the terminal shrinks midgame, a resize prompt replaces
the board while engine requests continue to be monitored.
Human input and new engine searches wait until the board fits again; an in-flight search retains
its deadline and may finish during resizing.

### Debug
Debug offers Play vs engine, Engine vs engine and Back. Both game types reuse the normal
setup, rules, input, pause, cancellation and move validation. A DEBUG board reserves four
status rows for the current engine's elapsed milliseconds, nodes, TT hits, NPS, depth, best
move and score. Only the latest snapshot is displayed, refreshed at the normal UI polling
interval; no progress history is appended. The required height is `board size + 16` rows.

Until valid metrics arrive (or if the engine does not support `DEBUG_OUTPUT`), the status
remains Thinking. Each new search clears the preceding search's metrics. Null best moves
and scores display `-`; best moves use board coordinates such as A1.
On returning from a debug game, a message shows its JSON transcript path before returning
to the Debug menu. See [docsEngine.md](docsEngine.md#debug-metrics-and-transcripts).

## Validation
The Windows console smoke test launches the actual application, injects native keyboard events
and inspects the active console viewport. It covers registry CRUD, setup, coordinate input, arrow
input, a human/engine exchange, an engine/engine match, a two-game test mode run with its JSON
report, debug metrics changing in place, unsupported-engine fallback, debug JSON transcripts
and normal exit.