# Build and Release Reference
Configure CMake from the repository root. The root project builds the core library,
protocol executables, tests, Python bindings, and NNUE tools according to its feature
options. Emscripten builds the extended protocol engine as WebAssembly in a separate
build tree.

## 1. Requirements and Defaults
- The project declares CMake 3.15 as its minimum.
- A compiler supporting C++17 is required.
- Native builds require the platform thread library.
- Test builds fetch GoogleTest v1.14.0 from GitHub.
- WebAssembly builds require an installed and activated Emscripten SDK.

When `CMAKE_BUILD_TYPE` is unset for a single-configuration generator, the root
project sets it to `Release`. `MOKU_BUILD_WASM` defaults to `OFF`; native builds do
not need Emscripten.

## 2. Configure, Build, and Test
From the repository root, configure and build the native project:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure --no-tests=error
```

To build the WebAssembly target separately, first activate the Emscripten SDK
environment, then run the matching platform script:

```powershell
.\scripts\build_wasm.ps1
```

```bash
bash scripts/build_wasm.sh
```

These scripts configure `build/wasm` with `MOKU_BUILD_WASM=ON`, disable unrelated
targets, and build `extended_moku_wasm`. The CMake cache is isolated from the native
build in `build/`. The generated JavaScript and WebAssembly module are located under
`build/wasm/engine/moku/`.

The full `scripts/build.ps1` and `scripts/build.sh` scripts build the native project
and then invoke the matching WASM script. They therefore require Emscripten to be
active as well. Use direct CMake commands for a native-only build.

## 3. Emscripten Environment

Install and activate the SDK:

```sh
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
```

On Linux or macOS, source `emsdk_env.sh` in each build shell:

```sh
source /path/to/emsdk/emsdk_env.sh
cd /path/to/moku
bash scripts/build_wasm.sh
```

On Windows, run `emsdk_env.bat` in Command Prompt and launch PowerShell from that
same prompt so it inherits the configured environment:

```bat
cd /d C:\tools\emsdk
emsdk.bat install latest
emsdk.bat activate latest
emsdk_env.bat
powershell.exe -NoProfile -ExecutionPolicy Bypass -File C:\path\to\moku\scripts\build_wasm.ps1
```

The `emcmake` command must be available in the active environment.

## 4. Targets
The active engine CMake project provides `moku_core`, `gomocup_moku`,
`extended_moku`, `moku_tests`, `gomocup_protocol_tests`, and
`extended_moku_wasm`. The WASM target is only enabled when `MOKU_BUILD_WASM=ON`
and CMake is configured through Emscripten.

## 5. Runtime Use and Formatting
Run a native executable and send protocol commands through stdin. Use `INFO` for
supported runtime settings; see [configuration](docsConfig.md) and
[protocol documentation](docsGomocupProtocol.md).

The repository's clang-format configuration uses LLVM style with four-space
indentation, Allman braces, a 100-character column limit, and left-aligned
pointers and references.

## 6. CI and Release Workflow
The `Build and Release` workflow runs for `v*` tags and manual dispatch.
Its build matrix uses `ubuntu-latest` and `windows-latest`.

The workflow:

1. Checks out the repository and prepares platform build tools.
2. Configures the project in `moku` with build directory `build`.
3. Builds `gomocup_moku` and `extended_moku`.
4. Copies version-suffixed binaries to a release staging directory.
5. Uploads artifacts named `moku-linux` and `moku-windows`.
6. Downloads both artifacts in a dependent job and invokes the GitHub Release action.

The version suffix is derived from the Git ref name with a leading `v` removed.
It is not read from `GameConfig::version` or the CMake project version.
Manual dispatch is enabled, but creating a release still requires a suitable
tag/release context; dispatch alone does not synchronize version values.

## 7. Implementation Locations
- [Root CMakeLists.txt](../../CMakeLists.txt): project options, dependencies, and build routing.
- [Engine CMakeLists.txt](../moku/CMakeLists.txt): engine and WASM targets.
- [build.yml](../../.github/workflows/build.yml): build matrix and release jobs.
- [Build scripts](../../scripts): native and WebAssembly convenience scripts.
- [.clang-format](../.clang-format): formatting rules.
- [Test reference](docsTests.md): focused validation commands.
- [Project README](../../README.md): quick-start instructions.