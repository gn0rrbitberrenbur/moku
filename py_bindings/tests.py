import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "build"))
os.add_dll_directory(r"C:\Util\Strawberry\c\bin")
os.add_dll_directory(os.path.dirname(sys.executable))

import numpy as np
import pymoku

pymoku.set_board_size(15)
assert pymoku.get_board_size() == 15
if pymoku.get_board_size() == 15:
    print("[SUCCESS] Board size is 15, as expected.")
else:
    print("[ERROR] Board size is not 15, something went wrong.")

b = pymoku.Board()
assert b.size() == 15
assert b.squares() == 225
if b.size() == 15 and b.squares() == 225:
    print("[SUCCESS] Board size and squares are correct.")
else:
    print("[ERROR] Board size or squares are incorrect.")

idx = b.pos(7, 7)
assert idx == 7 * 15 + 7
assert pymoku.algebraic_to_index("H8") == idx
if idx == 7 * 15 + 7 and pymoku.algebraic_to_index("H8") == idx:
    print("[SUCCESS] Index calculation is correct.")
else:
    print("[ERROR] Index calculation is incorrect.")

assert b.test_pos(idx) is False
b.make_move(idx, True)
assert b.test_pos(idx) is True
if b.test_pos(idx) is True:
    print("[SUCCESS] Move made successfully.")
else:
    print("[ERROR] Move was not made successfully.")

assert len(b.legal_moves()) == 224
assert idx not in b.legal_moves()
if len(b.legal_moves()) == 224 and idx not in b.legal_moves():
    print("[SUCCESS] Legal moves are correct after making a move.")
else:
    print("[ERROR] Legal moves are incorrect after making a move.")

state = b.to_numpy()
assert isinstance(state, np.ndarray)
assert state.shape == (2, 15, 15)
assert state.dtype == np.int8
assert state[0, 7, 7] == 1
assert state[1, 7, 7] == 0
if isinstance(state, np.ndarray) and state.shape == (2, 15, 15) and state.dtype == np.int8 and state[0, 7, 7] == 1 and state[1, 7, 7] == 0:
    print("[SUCCESS] Board state is correct after making a move.")
else:
    print("[ERROR] Board state is incorrect after making a move.")

b.undo_move(idx)
assert b.test_pos(idx) is False
assert len(b.legal_moves()) == 225
if b.test_pos(idx) is False and len(b.legal_moves()) == 225:
    print("[SUCCESS] Move undone successfully.")
else:
    print("[ERROR] Move was not undone successfully.")

assert b.check_win() is False
for c in range(5):
    b.make_move(b.pos(0, c), True)
assert b.check_win() is True
if b.check_win() is True:
    print("[SUCCESS] Win condition detected correctly.")
else:
    print("[ERROR] Win condition not detected correctly.")

w = pymoku.Board()
agent = pymoku.MinimaxAgent(4)
assert agent.get_max_depth() == 4
if agent.get_max_depth() == 4:
    print("[SUCCESS] Agent max depth is correct to 4.")
else:
    print("[ERROR] Agent max depth is incorrect to 4.")

agent.set_max_depth(2)
assert agent.get_max_depth() == 2
if agent.get_max_depth() == 2:
    print("[SUCCESS] Agent max depth set correctly to 2.")
else:
    print("[ERROR] Agent max depth not set correctly to 2.")

w.make_move(w.pos(7, 7), True)
mv = agent.get_best_move(w, False)
assert mv in w.legal_moves()
if mv in w.legal_moves():
    print("[SUCCESS] Best move is legal.")
else:
    print("[ERROR] Best move is not legal.")

mv2 = agent.get_best_move_timed(w, False, 1000)
assert mv2 in w.legal_moves()
if mv2 in w.legal_moves():
    print("[SUCCESS] Timed best move is legal.")
else:
    print("[ERROR] Timed best move is not legal.")

print("[TEST] nodes_searched:", agent.get_nodes_searched())
print("[TEST] tt_hits:", agent.get_tt_hits())

agent.clear_tt()

try:
    pymoku.set_board_size(99)
    raise AssertionError("expected exception")
except Exception as e:
    print("set_board_size error ok:", e)

tt = pymoku.TranspositionTable()
hb = pymoku.Board()
h0 = tt.compute_hash(hb, True)
move = hb.pos(7, 7)
h1 = tt.update_hash(h0, move, True)
hb.make_move(move, True)
h1_ref = tt.compute_hash(hb, False)
assert h1 == h1_ref, f"Zobrist mismatch: {h1} != {h1_ref}"
print("[SUCCESS] Zobrist update/compute consistent.")

print("Output of board:")
b.output_board()