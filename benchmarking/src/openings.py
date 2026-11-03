from __future__ import annotations

import hashlib
import json
import random
from pathlib import Path

SIZE = 15
OPENING_COUNT = 100
SEED = 20261103
Move = tuple[int, int]


class Board:
    def __init__(self) -> None:
        self.moves: list[Move] = []
        self.cells: dict[Move, int] = {}

    @property
    def turn(self) -> int:
        return len(self.moves) % 2 + 1

    def play(self, move: Move) -> bool:
        x, y = move
        if not 0 <= x < SIZE or not 0 <= y < SIZE or move in self.cells:
            raise ValueError(f"illegal move: {move}")
        color = self.turn
        self.cells[move] = color
        self.moves.append(move)
        return self.line_length(move) >= 5

    def line_length(self, move: Move) -> int:
        color = self.cells[move]
        x, y = move
        longest = 1
        for dx, dy in ((1, 0), (0, 1), (1, 1), (1, -1)):
            length = 1
            for sign in (-1, 1):
                step = 1
                while self.cells.get((x + sign * step * dx, y + sign * step * dy)) == color:
                    length += 1
                    step += 1
            longest = max(longest, length)
        return longest


def canonical(moves: list[Move]) -> tuple[Move, ...]:
    variants = []
    for swap in (False, True):
        for sx in (-1, 1):
            for sy in (-1, 1):
                transformed = []
                for x, y in moves:
                    x, y = x - 7, y - 7
                    if swap:
                        x, y = y, x
                    transformed.append((7 + sx * x, 7 + sy * y))
                # Positions, rather than move orders, determine duplicate openings.
                variants.append(tuple(sorted(transformed[::2]) + sorted(transformed[1::2])))
    return min(variants)


def generate_openings() -> dict[str, object]:
    rng = random.Random(SEED)
    openings: list[dict[str, object]] = []
    seen: set[tuple[Move, ...]] = set()
    while len(openings) < OPENING_COUNT:
        board = Board()
        board.play((7, 7))
        for _ in range(7):
            candidates = [
                (x, y) for y in range(3, 12) for x in range(3, 12)
                if (x, y) not in board.cells
                and any(max(abs(x - a), abs(y - b)) <= 2 for a, b in board.moves)
            ]
            board.play(rng.choice(candidates))
        if any(board.line_length(move) >= 4 for move in board.moves):
            continue
        key = canonical(board.moves)
        if key in seen:
            continue
        seen.add(key)
        openings.append({"id": f"opening-{len(openings) + 1:03}", "moves": board.moves})
    return {
        "schema_version": 1, "board_size": SIZE, "rule": "freestyle",
        "seed": SEED, "generator": "moku-local-v1",
        "provenance": "Synthetic positions generated locally; not a calibrated opening book.",
        "openings": openings,
    }


def load_openings(path: Path) -> list[tuple[str, list[Move]]]:
    data = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(data, dict):
        raise ValueError("opening file must contain an object")
    if (data.get("schema_version") != 1 or data.get("board_size") != SIZE
            or data.get("rule") != "freestyle"):
        raise ValueError("openings require schema 1, board_size 15, rule freestyle")
    items = data.get("openings")
    if not isinstance(items, list) or len(items) != OPENING_COUNT:
        raise ValueError("exactly 100 openings are required")
    result: list[tuple[str, list[Move]]] = []
    ids: set[str] = set()
    seen: set[tuple[Move, ...]] = set()
    for item in items:
        if not isinstance(item, dict):
            raise ValueError("each opening must be an object")
        name, moves = item.get("id"), item.get("moves")
        if not isinstance(name, str) or not name or name in ids:
            raise ValueError("opening IDs must be unique nonempty strings")
        if not isinstance(moves, list) or not moves or len(moves) >= SIZE * SIZE:
            raise ValueError(f"{name}: a nonempty, non-full opening is required")
        board = Board()
        for move in moves:
            if (not isinstance(move, list) or len(move) != 2
                    or not all(type(v) is int for v in move)):
                raise ValueError(f"{name}: moves must be integer [x, y] coordinates")
            if board.play((move[0], move[1])):
                raise ValueError(f"{name}: opening already contains a win")
        key = canonical(board.moves)
        if key in seen:
            raise ValueError(f"{name}: duplicate opening under rotation/reflection")
        ids.add(name)
        seen.add(key)
        result.append((name, board.moves))
    return result


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()
