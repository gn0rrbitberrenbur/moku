# Transposition Table Reference
The transposition table caches search results for positions reached through
different move orders. Position identity uses 64-bit Zobrist hashing with
separate keys for black stones, white stones, and the side to move.

## 1. Storage and Entry Format
The implementation is a fixed, direct-mapped vector, not an `unordered_map`.

| Constant | Value |
| --- | --- |
| `TT_BITS` | 23 |
| `TT_SIZE` | `1 << 23`, or 8388608 slots |
| `TT_MASK` | `TT_SIZE - 1` |

A hash selects its slot using `hash & TT_MASK`. Each agent owns its own table.
Storage is allocated during construction, so creating an agent has a substantial
memory cost: approximately `TT_SIZE * sizeof(TTEntry)`, plus overhead.
`g_config.max_memory` does not resize or limit this allocation.

| `TTEntry` field | Meaning |
| --- | --- |
| `key` | Full 64-bit position hash |
| `score` | Cached black-positive score |
| `depth` | Remaining search depth represented by the entry |
| `flag` | `EXACT`, `LOWERBOUND`, or `UPPERBOUND` |
| `best_move` | Cached move index, or `-1` |

## 2. Public Interface
| Method | Behavior |
| --- | --- |
| `compute_hash(board, is_black_turn)` | Builds a hash from the active board and side |
| `update_hash(hash, move, is_black_turn)` | XORs a move's color key and toggles the turn key |
| `probe(hash, entry, ply)` | Copies a matching entry and adjusts mate-like scores |
| `store(hash, score, depth, flag, best_move, ply)` | Writes an entry under the replacement policy |
| `clear()` | Replaces every slot with a default entry |
| `size()` | Returns allocated slots, not occupied entries |

For `update_hash()`, the boolean means the color of the stone being toggled;
the implementation names it `is_black`. Because XOR is reversible, the same
operation can undo that move and turn change if called consistently.

## 3. Zobrist Initialization
Construction uses `std::mt19937_64` with seed `12345`. Full hashing XORs keys
for occupied squares and includes the turn key when black is to move.
An empty white-to-move board hashes to zero.

Keys are initialized only for `g_config.squares()` at construction time.
The board size is not independently encoded in the hash, and changing global
dimensions does not reinitialize existing keys. Configure size before creating
a table or agent and reconstruct it when switching dimensions. `clear()` only
resets entries; it does not initialize additional keys.

This also affects protocol engines constructed before a larger `START` size:
accepting the command does not rebuild their agent's table.

## 4. Replacement and Bounds
`store()` replaces a slot when its full key differs or the new depth is at least
the stored depth. Thus a different colliding position can replace a deeper entry;
the depth preference only protects entries with the same key.

`probe()` checks the full key but does not decide whether the depth or flag is
sufficient for a search cutoff. The search layer performs that decision.

Scores at or beyond `+90000` or `-90000` are treated as mate-like values.
Storage normalizes them with the supplied ply, and probing adjusts them for
the caller's ply. Regular evaluation scores are returned unchanged.

## 5. Usage Example and Constraints
```cpp
#include "tt/transposition_table.hpp"

Board board;
TranspositionTable table;
const uint64_t hash = table.compute_hash(board, true);
table.store(hash, 12.0f, 3, TTEntry::EXACT, board.pos(7, 7), 0);

TTEntry entry;
const bool found = table.probe(hash, entry, 0);
```

Moves and dimensions must already be valid. The table does not validate move
legality or provide thread-safe entry access. The SMP search currently shares
these ordinary entries without synchronization.

Default entries have key zero and depth `-1`; there is no separate occupancy
flag. A probe for hash zero can therefore match a cleared slot. Consumers must
not equate every key match with a usable completed search result.

## 6. Implementation Locations
- [transposition_table.hpp](../moku/tt/transposition_table.hpp): entry type, capacity, and API.
- [transposition_table.cpp](../moku/tt/transposition_table.cpp): hashing, replacement, and normalization.
- [Search implementation](../moku/search/minimax.cpp): depth and bound interpretation.
- [Core tests](../moku/tests/tests.cpp): `TranspositionTableTest` suite.