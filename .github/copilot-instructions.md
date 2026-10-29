# Repository Instructions

## Project And Ownership

moku is an MIT-licensed C++17 Gomoku engine with protocol executables, Python bindings,
and a self-play dataset generator. The evaluator uses tactical patterns and positional
scores. Search uses alpha-beta minimax, iterative deepening, a Zobrist transposition
table, and a Lazy SMP entry point. The NNUE directory produces training data.

Repository-relative paths:

| Path | Responsibility |
| --- | --- |
| `CMakeLists.txt` | Root configuration, feature options, dependency setup, subdirectories |
| `VERSION` | Root CMake project and Python package version, in `major.minor.patch` form |
| `cmake/Dependencies.cmake` | GoogleTest and pybind11 discovery/FetchContent helpers |
| `cmake/CompilerWarnings.cmake` | Target-local warning flags and warnings-as-errors option |
| `engine/moku/` | Active engine CMake targets and C++ implementation |
| `engine/moku/game/`, `engine/moku/eval/`, `engine/moku/search/`, `engine/moku/tt/` | Board, evaluation, search, hashing/cache |
| `engine/moku/engine/` | Gomocup and extended stdin/stdout protocol handlers |
| `engine/moku/benchmark/`, `engine/moku/tests/` | Benchmark workloads and engine GoogleTest suites |
| `engine/docs/` | Module references; entry point `docsOverview.md` |
| `engine/CMakeLists.txt` | Additional engine build description; inspect root routing before editing |
| `nnue/generate_dataset/` | Self-play CLI, binary shard writer, NumPy reader, core contract tests |
| `python_bindings/src/bindings.cpp` | pybind11 API definitions |
| `python_bindings/python/pymoku/` | Python package, extension re-export, typing marker |
| `python_bindings/tests/` | pytest coverage for the extension |
| `pyproject.toml` | Python metadata, scikit-build-core, wheel and cibuildwheel settings |
| `scripts/` | Paired PowerShell/Bash build and CTest convenience scripts |
| `.github/workflows/build.yml` | Ubuntu/Windows engine artifact and tagged-release workflow |
| `.clang-format` | Repository C++ formatting configuration |

Read the relevant scoped instructions before editing:

- `.github/instructions/engine.instructions.md` for `engine/**`.
- `.github/instructions/nnue_dataset_generation.instructions.md` for `nnue/**`.
- `.github/instructions/python_bindings.instructions.md` for `python_bindings/**`.

## Toolchain And Dependencies

- Configure the complete repository with CMake 3.15 or newer and a C++17 compiler.
	The root adds `engine/moku`; its `moku_core` target publishes C++17 and PIC.
- `moku_core` has the alias `moku::core`. Python and NNUE link this local library.
- GoogleTest is fetched at `v1.14.0` when tests are enabled. pybind11 discovery first
	uses an installed CMake package, then FetchContent at `v3.0.1`. Dependency downloads
	require network access or provisioned FetchContent source overrides.
- Python metadata requires Python >=3.10, `scikit-build-core>=0.10`, `pybind11>=3.0`,
	and runtime `numpy<2.5`. The `test` extra adds pytest. cibuildwheel selects CPython
	3.10 through 3.14; distinguish this wheel matrix from the Python minimum.
- `VERSION` supplies root/package metadata. Runtime `GameConfig::version` uses the
	`MOKU_VERSION` compile definition, with `"unknown"` as its header fallback. Verify
	the target's compile definitions when changing version reporting.

## Build And Test

Run commands from the repository root. Each of `MOKU_BUILD_ENGINE`, `MOKU_BUILD_TESTS`,
`MOKU_BUILD_PYTHON`, and `MOKU_BUILD_NNUE` defaults to `ON`. The local core is built
independently of these feature switches. `MOKU_WARNINGS_AS_ERRORS` defaults to `OFF`.
For a top-level single-configuration build, the default configuration is `Release`.

The verification commands below use CMake/CTest 3.20 or newer for `--test-dir` support.
The declared project minimum is 3.15.

Full configure/build and C++ test discovery:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

Engine-only verification, with a separate build directory to isolate cached options:

```sh
cmake -S . -B build-engine -DCMAKE_BUILD_TYPE=Release -DMOKU_BUILD_ENGINE=ON -DMOKU_BUILD_TESTS=ON -DMOKU_BUILD_PYTHON=OFF -DMOKU_BUILD_NNUE=OFF
cmake --build build-engine --config Release --target gomocup_moku extended_moku tests gomocup_protocol_tests
ctest --test-dir build-engine -C Release --output-on-failure --no-tests=error
```

Other active targets are `_core`, `nnue_gen`, and, with tests enabled, `nnue_tests`.
Use the area instructions for their focused checks. GoogleTest discovery registers
suite/case names; inspect them with `ctest --test-dir <build-dir> -C Release -N`
before choosing a `-R` expression. Verify that the expected tests actually execute.

Engine binaries reside under `<build-dir>/engine/moku/`; the NNUE executable resides
under `<build-dir>/nnue/generate_dataset/`. Multi-configuration generators add a
configuration subdirectory, for example `Release/`, and Windows uses `.exe`.

`scripts/build.ps1` and `scripts/build.sh` configure and build all features in `build/`.
Specialized build scripts reconfigure that same directory with their own feature
switches. Test runners operate on the existing build directory. Check each script's
options, target, CTest expression, and generator configuration before relying on it;
the commands above describe the active root target graph.

The workflow declares tag (`v*`) and manual triggers, Ubuntu/Windows engine builds,
artifact uploads, and a release job. Run relevant tests explicitly as part of local
verification. Treat workflow and packaging configuration as code: validate paths
against the current tree and exercise the affected operation before claiming success.

## Code Style

- Use `.clang-format`: LLVM base, four spaces, no tabs, 100 columns, Allman braces,
	left-aligned pointers/references, regrouped and case-sensitively sorted includes.
	Only empty short functions stay on one line; short `if` statements stay multiline.
	Use the checked-in configuration rather than reformatting unrelated code.
- Match neighboring naming: `snake_case` functions/variables, `PascalCase` types,
	`UPPER_SNAKE_CASE` constants/macros. Use `#pragma once` in new C++ headers.
- Keep headers self-contained, preserve const-correctness, and qualify standard-library
	names. The warning helper uses `/W4` on MSVC or `-Wall -Wextra -pedantic` elsewhere.
- Follow nearby Python style: four-space indentation and `snake_case` functions.
	Keep Python wrappers and C++ binding signatures consistent.
- Describe nontrivial interfaces above the function. Preserve existing parameter/
	return documentation conventions. Keep comments factual and technically useful.

## Agent Workflow

1. Read the applicable instructions, owning implementation, nearest tests, and module
	 documentation. Trace the active call/build path; use source and configuration as
	 evidence for behavior. Identify a small check that can falsify the proposed fix.
2. Make small, surgical changes at the owning abstraction. Preserve unrelated edits,
	 public contracts, protocol output, and performance-sensitive behavior. Add source
	 files to every applicable target; prefer existing helpers and tests.
3. Run the cheapest relevant behavior check immediately after the first meaningful
	 edit, then the focused tests/build required by the affected interfaces. For
	 instruction-only edits, validate frontmatter, globs, paths, commands, and factual
	 claims. Keep generated builds, dependencies, datasets, and wheels out of edits.
4. Review documentation and instruction files on every task. Update every affected
	 reference in the same change: root/module README, `engine/docs/`, these instructions,
	 and scoped instructions as applicable. Preserve accurate text without cosmetic churn.
	 For engine modules, use `docsModuleName.md`, follow `docsConfig.md`, and maintain
	 `docsOverview.md` links when module ownership or structure changes.
5. Explain what changed, why it is necessary, why this implementation fits the existing
	 design, and which checks ran. State any unverified behavior or concrete blockers.
	 Propose unrelated defects separately instead of expanding the implementation scope.

## Documentation And Instruction Style

Write all documentation, API descriptions, and instruction files in English, with a
technical, precise, present-tense style. State current responsibilities, contracts,
inputs, outputs, defaults, and commands. Ground claims in inspected implementation.
Express constraints as positive contracts, for example "Supported board sizes are
5 through 19" or "Configure from the repository root". Keep historical comparisons,
change announcements, speculative capabilities, and failure narratives out of permanent
reference text. Put change rationale and observed verification failures in the task
report rather than the reference documentation. Distinguish declared configuration
from successfully executed build, import, or test evidence.
