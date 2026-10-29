# Build and Release Reference
The CMake project lives under `moku`. It builds a static core library, two protocol executables, and two GoogleTest executables.

## 1. Requirements and Defaults
- The project declares CMake 3.14 as its minimum.
- A compiler supporting C++17 is required.
- CMake must find the platform thread library.
- Standalone configuration fetches GoogleTest v1.14.0 from GitHub.

When `CMAKE_BUILD_TYPE` is unset, the project sets it to `Release`.

MSVC builds enable `/W4`; other compilers receive `-Wall -Wextra -pedantic`.
Platform definitions are `_WIN32`, `LINUX`, or `MACOS`, as applicable.

## 2. Target Layout
| Target | Type | Included functionality |
| --- | --- | --- |
| `moku_core` | Static library | Board, coordinate conversion, evaluation, search, and table |
| `gomocup_moku` | Executable | Core sources and Gomocup handler |
| `extended_moku` | Executable | Core sources, extended handler, and benchmarks |
| `tests` | Executable | Core, benchmark, Gomocup handler, and core test source |
| `gomocup_protocol_tests` | Executable | Board, evaluation, search, table, handler, and protocol test source |

Executables compile their listed core sources directly rather than linking
`moku_core`. New source files must therefore be added to every applicable
target, not only to the library.

## 3. Configure, Build, and Test
From the repository root, in PowerShell:

```powershell
cmake -S moku -B moku\build -DCMAKE_BUILD_TYPE=Release
cmake --build moku\build --config Release
ctest --test-dir moku\build -C Release --output-on-failure
```

From the repository root, the build can also be started with these scripts:

```powershell
.\scripts\build.ps1
```

```bash
bash scripts/build.sh
```

To build only the engines:

```powershell
cmake --build moku\build --config Release --target gomocup_moku extended_moku
```

Typical multi-configuration Windows output locations are:

```text
moku\build\Release\gomocup_moku.exe
moku\build\Release\extended_moku.exe
```

## 4. Runtime Use and Formatting
Run an executable and send protocol commands through stdin. There are no
command-line configuration flags. Use `INFO` for supported runtime settings;
see [configuration](docsConfig.md) and [protocol documentation](docsGomocupProtocol.md).

The repository's clang-format configuration uses LLVM style with four-space
indentation, Allman braces, a 100-character column limit, and left-aligned
pointers and references.

## 5. CI and Release Workflow
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

## 6. Implementation Locations
- [CMakeLists.txt](../moku/CMakeLists.txt): targets, dependencies, and compiler settings.
- [build.yml](../.github/workflows/build.yml): build matrix and release jobs.
- [.clang-format](../.clang-format): formatting rules.
- [Test reference](docsTests.md): focused validation commands.
- [Project README](../README.md): quick-start instructions.