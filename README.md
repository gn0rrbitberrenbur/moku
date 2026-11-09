# moku
Moku is a C++17 Gomoku engine.

## Project overview
The engine uses a minimax-style search to find the best moves. It is designed to run as a protocol-based engine: commands are read from standard input and results are written to standard output.

The repository is organized as a CMake project and supports building engine binaries, tests, Python bindings, and NNUE tools from a single root configuration.

## Repository structure
```
moku/
├── CMakeLists.txt - top-level CMake configuration and project-wide options
├── benchmarking/ — Paired engine matches, protocol logging, and Elo/SPRT strength comparison
├── cmake/ - shared CMake modules for compiler warnings and dependencies
├── cli/ - command-line application and terminal UI for playing and testing the engine
│   └── moku-cli/
│       ├── board/ - board state and move handling for the CLI
│       ├── engine/ - engine integration and protocol communication
│       ├── file_handling/ - loading and saving configuration or game data
│       ├── process/ - process management for engine lifecycles
│       ├── settings/ - CLI settings and configuration management
│       ├── tests/ - CLI-specific test suite
│       ├── ui/ - terminal rendering, input handling, and menus
│       └── main.cpp - entry point for the CLI application
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
## Build
### Build and test scripts
The project provides separate convenience scripts for each platform. These scripts wrap the underlying CMake configure/build/test commands and are intended to make local development easier.

To get started with the scripts see [the documentation](scripts/docs_scripts.md)

### Build the complete project with CMake
Configure the project from the repository root. By default, CMake enables the
engine, tests, Python bindings, and NNUE dataset generator; WebAssembly is disabled.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

To build only selected components, set the corresponding `MOKU_BUILD_ENGINE`, `MOKU_BUILD_TESTS`, `MOKU_BUILD_PYTHON`, or `MOKU_BUILD_NNUE`
CMake option to `OFF` during configuration.

### WebAssembly build
The project can build the `extended_moku` engine for WebAssembly using Emscripten.

Make sure you have Emscriptem installed on your system ([See](https://emscripten.org/docs/getting_started/downloads.html))

The full build scripts also run this build, so activate the Emscripten SDK environment before running `scripts/build.ps1` or `scripts/build.sh`.

The dedicated `scripts/build_wasm.ps1` and `scripts/build_wasm.sh` scripts build only the WebAssembly target.

Both use `build/wasm` as a separate CMake build directory, the generated `extended_moku_wasm.js` and `.wasm` files are under `build/wasm/engine/moku/`.