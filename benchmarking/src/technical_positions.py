from __future__ import annotations

from .openings import Board, Move


def positions() -> list[tuple[str, str, list[Move]]]:
    """Return deterministic legal positions exercising different search workloads."""
    setups = [
        ("early", "Early open position", [(7, 7), (8, 8)]),
        ("attack", "Immediate attacking threat", [
            (5, 7), (5, 8), (6, 7), (6, 8), (7, 7), (9, 8), (8, 7), (10, 9),
        ]),
        ("defense", "Immediate defensive threat", [
            (5, 7), (5, 8), (6, 7), (6, 8), (7, 7), (9, 8), (8, 7),
        ]),
        ("clustered", "Clustered midgame", [
            (7, 7), (8, 7), (7, 8), (8, 8), (6, 7), (9, 7), (6, 8), (9, 8),
            (7, 6), (8, 9), (6, 6), (9, 9),
        ]),
        ("wide", "Dispersed high-branching position", [
            (4, 4), (10, 10), (4, 10), (10, 4), (7, 7), (2, 7), (12, 7), (7, 2),
            (7, 12), (2, 2), (12, 12), (2, 12),
        ]),
        ("edge", "Edge and corner interactions", [
            (0, 0), (1, 0), (0, 1), (2, 0), (1, 1), (2, 1),
            (0, 2), (3, 0), (3, 1), (1, 2),
        ]),
    ]
    for name, _, moves in setups:
        board = Board()
        for move in moves:
            if board.play(move):
                raise ValueError(f"{name}: position contains an earlier win")
    return setups
