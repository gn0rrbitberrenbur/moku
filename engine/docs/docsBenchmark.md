# Benchmark Module Reference
The benchmark module constructs predefined positions, invokes search, and
reports throughput and table-hit statistics. 

It is available programmatically
and through `BENCHMARK` in the extended executable.

## 1. Public Interface
```cpp
BenchResult run_position(
    const std::string& name,
    const std::vector<std::pair<std::string, bool>>& setup,
    bool side_to_move, int depth, int time_ms, int board_size = 15);

int run_benchmark(int depth, int time_ms);
```

Setup entries pair an algebraic square with a color: 
- `true` is black,
- `false` is white. 

`side_to_move` uses the same convention.

`run_position()` temporarily sets the global board size, builds the position,
constructs a new agent, clears its table, and times the search call.

Positive `time_ms` selects timed SMP search, zero or negative values select fixed-depth
search at this API level. 

The protocol wrapper rejects negative limits.

The previous board size is restored on normal return.

## 2. Predefined Workloads
| Name | Board size | Side to move | Setup purpose |
| --- | --- | --- | --- |
| `empty_center` | 15 | White | A single black stone at `H8`; not literally empty |
| `open_three` | 15 | White | Three connected black stones with other nearby stones |
| `block_four` | 15 | White | Black four requiring defensive consideration |
| `winning_four` | 15 | White | White four with a possible continuation |
| `midgame_dense` | 15 | Black | Clustered midgame position |
| `endgame_sparse` | 19 | Black | Widely distributed stones on a larger board |
| `tt_stress` | 15 | Black | Multiple groups intended to exercise cached positions |
| `branching_wide` | 15 | Black | Dispersed setup with many nearby candidates |

Each workload gets a fresh agent and table; this does not measure reuse of a
warm table across positions. `run_benchmark()` also disables debug output
globally without restoring the previous flag.

## 3. Result Fields
| `BenchResult` field | Current meaning |
| --- | --- |
| `name` | Workload label |
| `reached_depth` | Configured agent depth, **not measured completed depth** |
| `nodes` | Search node counter |
| `tt_hits` | Successful table-probe counter |
| `tt_size` | Currently assigned `0`, not measured |
| `time_s` | Wall-clock duration of the search call |
| `best_move` | Returned row-major index, or `-1` |
| `score` | Currently assigned `0`, not measured |

The timing starts after position setup, agent construction, and table clearing.
It therefore excludes those costs. The table columns printed by the runner are:

```text
position  depth  nodes  nodes/s  tt_hits  tt_rate  time_s  best
```

`nodes/s` is `nodes / time_s`, and `tt_rate` is `tt_hits / nodes`; zero
denominators yield zero. The rate is printed as a fraction, not a percentage.
Totals include node count, summed search time, and aggregate nodes per second.
`best` is an integer index, not algebraic notation or a Gomocup coordinate.

## 4. Usage Example
In an `extended_moku` session:

```text
BENCHMARK 4 1000
```

Programmatic single-position setup:

```cpp
#include "benchmark/benchmark.hpp"

const BenchResult result =
    run_position("single_center", {{"H8", true}}, false, 4, 1000, 15);
```

## 5. Validation and Limitations
The programmatic runner does not validate complete setups, turn counts,
duplicate squares, depth, or supported dimensions. It passes coordinate
conversion results directly to `make_move()`, so invalid algebraic strings
must not be supplied.

Reported depth is a requested limit; it cannot establish that timed search
completed that depth. The placeholder score and table size must not be used
as evaluation or occupancy measurements. Timed workloads inherit the current
SMP implementation's concurrency limitations.

## 6. Implementation Locations
- [benchmark.hpp](../moku/benchmark/benchmark.hpp): API and result structure.
- [benchmark.cpp](../moku/benchmark/benchmark.cpp): setups, timing, and report output.
- [Extended protocol](docsExtendedProtocol.md): command validation and invocation.
- [Search reference](docsSearch.md): counters, timing, and parallel behavior.