# Build and test scripts
The project provides separate convenience scripts for each platform. These scripts wrap the underlying CMake configure/build/test commands and are intended to make local development easier.

## Linux (.sh)
### All targets
- `bash scripts/build.sh` - Configures the project and builds the full default configuration in the build/ directory

### Engine targets
- `bash scripts/build_core.sh` - Builds only the core engine library
- `bash scripts/build_extended_moku.sh` - Builds the extended protocol executable
- `bash scripts/build_gomocup_moku.sh` - Builds the Gomocup protocol executable
- `bash scripts/build_nnue_gen.sh` - Builds the NNUE dataset generator
- `bash scripts/build_wasm.sh` - Builds the Webassembly target

### Test targets
- `bash scripts/build_moku_tests.sh` - Builds the core engine test target
- `bash scripts/build_gomocup_protocol_tests.sh` - Builds the Gomocup protocol test target
- `bash scripts/build_nnue_tests.sh` - Builds the NNUE-related tests

### Run tests
- `bash scripts/run_all_tests.sh` - Runs all configured tests via ctest
- `bash scripts/run_engine_tests.sh` - Runs the main engine test suite
- `bash scripts/run_gomocup_protocol_tests.sh` - Runs the Gomocup protocol tests
- `bash scripts/run_nnue_tests.sh` - Runs the NNUE test suite

## Windows (.ps1)
### All targets
- `./scripts/build.ps1` - Configures and builds the full project

### Engine targets
- `./scripts/build_core.ps1` - Builds only the core engine library
- `./scripts/build_extended_moku.ps1` - Builds the extended protocol executable
- `./scripts/build_gomocup_moku.ps1` - Builds the Gomocup protocol executable
- `./scripts/build_nnue_gen.ps1` - Builds the NNUE dataset generator
- `./scripts/build_wasm.ps1` - Builds the Webassembly target

### Test targets
- `./scripts/build_moku_tests.ps1` - Builds the core engine test target
- `./scripts/build_gomocup_protocol_tests.ps1` - Builds the Gomocup protocol test target
- `./scripts/build_nnue_tests.ps1` - Builds the NNUE-related tests

### Run tests
- `./scripts/run_all_tests.ps1` - Runs all configured tests via ctest
- `./scripts/run_engine_tests.ps1` - Runs the main engine test suite
- `./scripts/run_gomocup_protocol_tests.ps1` - Runs the Gomocup protocol tests
- `./scripts/run_nnue_tests.ps1` - Runs the NNUE test suite