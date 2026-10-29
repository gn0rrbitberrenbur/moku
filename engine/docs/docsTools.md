# Coordinate Tools Reference
The tools submodule converts human-readable algebraic coordinates to the integer
indices used by the board and search. It is used by benchmark position setup and
is distinct from the Gomocup `X,Y` parser.

## 1. Public Interface
```cpp
int algebraic_to_index(const std::string& pos, int board_size = 15);
```

The input consists of a column letter followed by a one-based row number.
Letters are case-insensitive. There is no skipped column letter.

| Input on a 15x15 board | Returned index |
| --- | --- |
| `A1` | `0` |
| `B1` | `1` |
| `A2` | `15` |
| `H8` | `112` |
| `O15` | `224` |

The conversion is `(row_number - 1) * board_size + column_index`.

## 2. Size Selection
Omitting the size argument uses the declaration's fixed default of **15**.
Passing a value of zero or less instead selects `g_config.board_size`.
These are different behaviors: omitting the argument does not automatically
follow a changed global size.

The converter checks coordinates against the selected dimensions but does not
enforce the board bitsets' 19x19 storage limit. Pass a supported size.

## 3. Validation and Return Value
The function returns `-1` when:

- The input has fewer than two characters.
- The numeric row cannot be converted to an integer.
- The column or row is outside the selected board.

Conversion uses `std::stoi` without checking how many characters were consumed.
Consequently, an input such as `H8extra` may be accepted as `H8`; this is not
a strict notation validator. Invalid input is returned as `-1` without logging.
Callers must check that result before using it as a bitset index.

## 4. Usage Example
```cpp
#include "game/tools/algebraic_to_index.hpp"

const int move = algebraic_to_index("H8", 15);
if (move >= 0)
{
    // Pass the validated index to board or search code.
}
```

For a 15x15 board, the equivalent Gomocup coordinate is `7,7`. No public
index-to-algebraic conversion is currently provided by this submodule.

## 5. Implementation Locations
- [algebraic_to_index.hpp](../moku/game/tools/algebraic_to_index.hpp): declaration and default size.
- [algebraic_to_index.cpp](../moku/game/tools/algebraic_to_index.cpp): parsing and conversion.
- [Benchmark implementation](../moku/benchmark/benchmark.cpp): predefined position setup.
- [Core tests](../moku/tests/tests.cpp): `AlgebraicConversion` suite.