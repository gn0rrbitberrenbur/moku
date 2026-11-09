# Transposition Table Reference
The transposition table caches search results for positions reached through
different move orders. Position identity uses 64-bit Zobrist hashing with
separate keys for black stones, white stones, and the side to move.

## 1. Storage and Entry Format
The implementation is a direct-mapped vector with power-of-two capacity, not an
`unordered_map`. The default and maximum capacity are `2^23` slots.

| Constant | Value |
| --- | --- |
| `TT_BITS` | 23 |
| `TT_SIZE` | `1 << 23`, or 8388608 slots |
| `tt_mask` | Current slot count minus one |

A hash selects its slot using `hash & tt_mask`. Each agent owns its own table.
Storage is allocated during construction, so creating an agent has a substantial
memory cost: approximately `TT_SIZE * sizeof(TTEntry)`, plus overhead.
An explicit byte budget selects the largest power-of-two slot count that fits,
capped at `TT_SIZE`. Zero selects the default capacity; a positive budget must
fit at least one `TTEntry`. The extended engine applies `INFO max_memory` to its
current agent and passes the budget to benchmark agents. The standard Gomocup
engine and default-constructed programmatic agents retain the default capacity.

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
| `TranspositionTable(max_memory_bytes = 0)` | Allocates a table using the supplied byte budget and initializes Zobrist keys |
| `compute_hash(board, is_black_turn)` | Builds a hash from the active board and side |
| `update_hash(hash, move, is_black_turn)` | XORs a move's color key and toggles the turn key |
| `probe(hash, entry, ply)` | Copies a matching entry and adjusts mate-like scores |
| `store(hash, score, depth, flag, best_move, ply)` | Writes an entry under the replacement policy |
| `clear()` | Replaces every slot with a default entry |
| `set_max_memory(max_memory_bytes)` | Applies the budget; changing capacity replaces storage, clears entries and occupancy, and preserves Zobrist keys |
| `size()` | Returns allocated slots, not occupied entries |
| `allocated_bytes()` | Returns vector capacity times `sizeof(TTEntry)`, excluding metadata and allocator overhead |
| `occupancy_permille()` | Returns integer floor of occupied slots times 1000 divided by capacity |

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

`probe()` requires an occupied slot and a matching full key but does not decide
whether the depth or flag is sufficient for a search cutoff. The search layer
performs that decision.

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

Moves and dimensions must already be valid; stored depths are nonnegative.
The table does not validate move legality. Probe/store operations synchronize
entry access using 256 striped mutexes. An atomic occupancy counter increments
only when a store fills an empty slot; updates and collision replacements leave
it unchanged. `clear()` requires no concurrent search and resets occupancy
without releasing capacity. Lock selection uses the table slot index so colliding
hashes synchronize even when capacity is smaller than the stripe count.

`set_max_memory()` also requires no concurrent search. If the selected capacity
is unchanged, it retains entries and occupancy. An undersized positive budget
throws `std::invalid_argument`; allocation failures propagate without changing the
table. The old vector is released after a successful replacement, so shrinking
reduces retained entry storage. Both vectors temporarily coexist during resizing;
the byte budget excludes this transient peak and all non-entry storage.

Default entries have depth `-1`, which marks an empty slot. A probe for hash zero
misses a cleared slot and succeeds after a real hash-zero entry is stored.
Debug search metrics expose capacity, allocated vector bytes, and occupancy;
see [docsSearch.md](docsSearch.md).

## 6. Implementation Locations
- [transposition_table.hpp](../moku/tt/transposition_table.hpp): entry type, capacity, and API.
- [transposition_table.cpp](../moku/tt/transposition_table.cpp): hashing, replacement, and normalization.
- [Search implementation](../moku/search/minimax.cpp): depth and bound interpretation.
- [Core tests](../moku/tests/tests.cpp): `TranspositionTableTest` suite.