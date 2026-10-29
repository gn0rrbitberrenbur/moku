import numpy as np
import pymoku
import pytest

# Test board initialization
def test_board_size():
    pymoku.set_board_size(15)
    assert pymoku.get_board_size() == 15

# Test if setting an invalid board size raises an exception
def test_invalid_board_size():
    with pytest.raises(Exception):
        pymoku.set_board_size(99)

# Test if board is initialized correctly (size equals size^2)
def test_board_initialization():
    b = pymoku.Board()
    assert b.size() == 15
    assert b.squares() == 225

# Test position conversion and algebraic notation
def test_position_conversion():
    b = pymoku.Board()
    idx = b.pos(7, 7)
    assert idx == 7 * 15 + 7
    assert pymoku.algebraic_to_index("H8") == idx

# Test making a move
def test_make_move():
    b = pymoku.Board()
    idx = b.pos(7, 7)
    assert b.test_pos(idx) is False
    b.make_move(idx, True)
    assert b.test_pos(idx) is True

# Test legal moves after making a move
def test_legal_moves_after_move():
    b = pymoku.Board()
    idx = b.pos(7, 7)
    b.make_move(idx, True)
    assert len(b.legal_moves()) == 224
    assert idx not in b.legal_moves()

# Test board state conversion to numpy array
def test_board_state_to_numpy():
    b = pymoku.Board()
    idx = b.pos(7, 7)
    b.make_move(idx, True)
    state = b.to_numpy()
    assert isinstance(state, np.ndarray)
    assert state.shape == (2, 15, 15)
    assert state.dtype == np.int8
    assert state[0, 7, 7] == 1
    assert state[1, 7, 7] == 0

# Test undoing a move
def test_undo_move():
    b = pymoku.Board()
    idx = b.pos(7, 7)
    b.make_move(idx, True)
    b.undo_move(idx)
    assert b.test_pos(idx) is False
    assert len(b.legal_moves()) == 225

# Test win condition
def test_win_condition():
    b = pymoku.Board()
    assert b.check_win() is False
    for c in range(5):
        b.make_move(b.pos(0, c), True)
    assert b.check_win() is True

# Test whether the agent max depth is set correctly
def test_agent_max_depth():
    b = pymoku.Board()
    agent = pymoku.MinimaxAgent(4)
    assert agent.get_max_depth() == 4

# Test if the agent returns a legal best move
def test_agent_returns_legal_best_move():
    w = pymoku.Board()
    agent = pymoku.MinimaxAgent(4)
    w.make_move(w.pos(7, 7), True)
    mv = agent.get_best_move(w, False)
    assert mv in w.legal_moves()

# Test transposition table hash computation and update
def test_transposition_table_hash():
    tt = pymoku.TranspositionTable()
    hb = pymoku.Board()
    h0 = tt.compute_hash(hb, True)
    move = hb.pos(7, 7)
    h1 = tt.update_hash(h0, move, True)
    hb.make_move(move, True)
    h1_ref = tt.compute_hash(hb, False)
    assert h1 == h1_ref, f"Zobrist mismatch: {h1} != {h1_ref}"

# Test row/col conversion
def test_row_col_conversion():
    cb = pymoku.Board()
    p = cb.pos(3, 9)
    assert cb.row(p) == 3
    assert cb.col(p) == 9

# Test wins_at method for detecting winning moves
def test_wins_at():
    wb = pymoku.Board()
    for c in range(4):
        wb.make_move(wb.pos(0, c), True)
    assert wb.wins_at(wb.pos(0, 4), True) is True
    assert wb.wins_at(wb.pos(5, 5), True) is False

# Test getting and setting game configuration
def test_game_config():
    cfg = pymoku.get_config()
    assert isinstance(cfg, pymoku.GameConfig)
    cfg.max_depth = 8
    assert pymoku.get_config().max_depth == 8
    cfg.time_limit_ms = 5000
    assert pymoku.get_config().time_limit_ms == 5000
    cfg.debug_output = True
    assert pymoku.get_config().debug_output is True
    cfg.debug_output = False
    assert cfg.squares() == cfg.board_size * cfg.board_size
    assert isinstance(cfg.version, str)
    assert cfg.cores >= 1

# Test evaluator methods
def test_evaluator_methods():
    ev = pymoku.Evaluator()
    eb = pymoku.Board()
    eb.make_move(eb.pos(7, 7), True)
    assert isinstance(ev.evaluate_board(eb), float)
    assert isinstance(ev.evaluate_board_stm(eb, True), float)
    vm = ev.get_valid_moves(eb)
    assert isinstance(vm, list)
    assert len(vm) > 0
    prio = ev.move_priority(eb, eb.pos(7, 8), False)
    assert isinstance(prio, int)
    ps = ev.position_score(eb.pos(7, 7))
    assert isinstance(ps, int)