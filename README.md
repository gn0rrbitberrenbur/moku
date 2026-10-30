# moku
Moku is a C++17 Gomoku engine.

## Project overview
The engine uses a minimax-style search to find the best moves. It is designed to run as a protocol-based engine: commands are read from standard input and results are written to standard output.

The repository is organized as a CMake project and supports building engine binaries, tests, Python bindings, and NNUE tools from a single root configuration.

## Repository structure
```
moku/
├── CMakeLists.txt - top-level CMake configuration and project-wide options
├── cmake/ - shared CMake modules for compiler warnings and dependencies
├── cli/   - cli application that runs gomocup compliant engines
├── engine/ - engine source tree and documentation
│   └── moku/
│       ├── game/ - board logic and game rules
│       ├── search/ - minimax and search logic
│       ├── eval/ - evaluator and move ordering
│       ├── tt/ - transposition table and hashing
│       ├── benchmark/ - benchmark utilities
│       └── tests/ - unit and protocol tests
├── python_bindings/ - Python bindings built with pybind11
├── nnue/
│   └── generate_dataset/ - dataset generation utilities for NNUE experiments
├── scripts/ - convenience scripts for building and running tests on Windows and Linux
├── VERSION - canonical project version
├── LICENSE - project license
├── README.md - project overview
└── .github/ - repository automation and project instructions
```

## Build and test scripts
The project provides separate convenience scripts for each platform. These scripts wrap the underlying CMake configure/build/test commands and are intended to make local development easier.

### Linux (.sh)
#### Build
```
bash scripts/build.sh
```
Configures the project and builds the full default configuration in the build/ directory

```
`bash scripts/build_core.sh` 
```
Builds only the core engine library

```
bash scripts/build_extended_moku.sh
```
Builds the extended protocol executable

```
bash scripts/build_gomocup_moku.sh
```
Builds the Gomocup protocol executable

```
bash scripts/build_moku_tests.sh
```
Builds the core engine test target

```
bash scripts/build_gomocup_protocol_tests.sh
```
Builds the Gomocup protocol test target

```
bash scripts/build_nnue_gen.sh
```
Builds the NNUE dataset generator

```
bash scripts/build_nnue_tests.sh
```
Builds the NNUE-related tests

#### Tests
```
bash scripts/run_all_tests.sh
```
Runs all configured tests via ctest

```
bash scripts/run_engine_tests.sh
```
Runs the main engine test suite

```
bash scripts/run_gomocup_protocol_tests.sh
```
Runs the Gomocup protocol tests

```
bash scripts/run_nnue_tests.sh
```
Runs the NNUE test suite

### Windows (.ps1)
#### Build
```
./scripts/build.ps1
```
Configures and builds the full project

```
./scripts/build_core.ps1
```
Builds only the core library

```
./scripts/build_extended_moku.ps1
```
Builds the extended protocol executable

```
./scripts/build_gomocup_moku.ps1
```
Builds the Gomocup protocol executable

```
./scripts/build_moku_tests.ps1
```
Builds the core engine tests

```
./scripts/build_gomocup_protocol_tests.ps1
```
Builds the Gomocup protocol tests

```
./scripts/build_nnue_gen.ps1
```
Builds the NNUE generator

```
./scripts/build_nnue_tests.ps1
```
Builds the NNUE tests

#### Tests
```
./scripts/run_all_tests.ps1
```
Runs all tests using the configured build tree

```
./scripts/run_engine_tests.ps1
```
Runs the main engine test suite

```
./scripts/run_gomocup_protocol_tests.ps1
```
Runs the Gomocup protocol tests

```
./scripts/run_nnue_tests.ps1
```
Runs the NNUE tests

## Typical workflow
For a standard local build on Linux:

```bash
bash scripts/build.sh
bash scripts/run_all_tests.sh
```

For a standard local build on Windows PowerShell:
```
./scripts/build.ps1
./scripts/run_all_tests.ps1
```