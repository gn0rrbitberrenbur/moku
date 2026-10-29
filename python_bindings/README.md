# pymoku

Python bindings for the [moku](https://github.com/gn0rrbitberrenbur/moku) board-game engine.

The project provides a C++17 engine exposed to Python through `pybind11`. It includes board management, move generation, a minimax agent, evaluation helpers, transposition tables, NumPy conversion, and benchmarking utilities.

## Requirements

- Python 3.10 or newer
- A C++17 compiler
- CMake 3.15 or newer
- NumPy
- Pytest

The C++ dependency `moku` is downloaded automatically by CMake from GitHub.

## Installation
### Building it yourself
Clone the repository and create a virtual environment.

Install the package:

```bash
python -m pip install .
```

On Linux, CMake selects a suitable generator automatically. On Windows, install the required Visual Studio C++ build tools.

## Basic Usage

```python
import pymoku

pymoku.set_board_size(15)

board = pymoku.Board()
center = board.pos(7, 7)

board.make_move(center, True)

print(board.size())
print(board.squares())
print(board.legal_moves())
print(board.check_win())
```

The `is_black` argument is a boolean:

- `True`: black player
- `False`: white player

## Board API

```python
board = pymoku.Board()
```

| Method | Description |
| --- | --- |
| `make_move(pos, is_black)` | Places a stone on a position |
| `undo_move(pos)` | Removes a stone from a position |
| `test_pos(pos)` | Checks whether a position is occupied |
| `legal_moves()` | Returns all currently available positions |
| `check_win()` | Checks whether the current board contains a winning line |
| `wins_at(pos, is_black)` | Checks whether placing a stone would win |
| `size()` | Returns the board width and height |
| `squares()` | Returns the total number of squares |
| `pos(row, col)` | Converts a row and column to an index |
| `row(pos)` | Returns the row for an index |
| `col(pos)` | Returns the column for an index |
| `to_numpy()` | Returns the board as a NumPy array |
| `output_board()` | Prints the board state |

The board size can be configured between 5 and 19:

```python
pymoku.set_board_size(15)
print(pymoku.get_board_size())
```

Algebraic notation can be converted to a board index:

```python
index = pymoku.algebraic_to_index("H8")
```

## NumPy Representation

`Board.to_numpy()` returns an array with shape `(2, board_size, board_size)` and dtype `numpy.int8`.

- Channel `0` contains black stones
- Channel `1` contains white stones

```python
state = board.to_numpy()

print(state.shape)
print(state.dtype)
```

## Minimax Agent

```python
agent = pymoku.MinimaxAgent(depth=4)

move = agent.get_best_move(board, is_black=False)
print(move)
```

A time-limited search can be used with:

```python
move = agent.get_best_move_timed(
    board,
    is_black=False,
    time_limit_ms=1000,
)
```

The agent also provides a parallel timed search:

```python
move = agent.get_best_move_timed_smp(
    board,
    is_black=False,
    time_limit_ms=1000,
)
```

Useful agent methods include:

```python
agent.set_max_depth(6)
print(agent.get_max_depth())

print(agent.get_nodes_searched())
print(agent.get_tt_hits())
print(agent.get_last_root_score())
print(agent.get_last_root_candidates())

agent.clear_tt()
```

Each root candidate contains:

```python
candidate.move
candidate.score
candidate.nodes
```

## Evaluator

The evaluator exposes board scoring and move-ordering helpers:

```python
evaluator = pymoku.Evaluator()

score = evaluator.evaluate_board(board)
score_stm = evaluator.evaluate_board_stm(board, black_to_move=True)
moves = evaluator.get_valid_moves(board)
priority = evaluator.move_priority(board, move, is_black=True)
position_score = evaluator.position_score(move)
```

## Game Configuration

Global engine configuration is available through `get_config()`:

```python
config = pymoku.get_config()

config.board_size = 15
config.max_depth = 6
config.time_limit_ms = 1000
config.debug_output = False

print(config.squares())
print(config.version)
print(config.cores)
```

## Transposition Table

```python
table = pymoku.TranspositionTable()

board = pymoku.Board()
hash_value = table.compute_hash(board, is_black_turn=True)

move = board.pos(7, 7)
updated_hash = table.update_hash(
    hash_value,
    move,
    is_black=True,
)

print(table.size())
table.clear()
```

## Benchmarks

Run a predefined benchmark:

```python
return_code = pymoku.run_benchmark(
    depth=4,
    time_ms=1000,
)

print(return_code)
```

Run a benchmark for a specific position:

```python
result = pymoku.run_position(
    name="example",
    setup=[
        ("H8", True),
        ("I9", False),
    ],
    side_to_move=True,
    depth=4,
    time_ms=1000,
)

print(result.name)
print(result.best_move)
print(result.reached_depth)
print(result.nodes)
print(result.tt_hits)
print(result.tt_size)
print(result.time_s)
print(result.score)
```

## Running Tests

Install the test dependencies:

```bash
python -m pip install pytest numpy
```

Then run the tests:

```bash
pytest -q
```