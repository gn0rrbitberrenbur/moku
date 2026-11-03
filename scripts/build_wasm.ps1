$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build\wasm'

if (-not (Get-Command emcmake -ErrorAction SilentlyContinue))
{
    [Console]::Error.WriteLine(
        '[ERROR] emcmake was not found. Activate the Emscripten SDK environment first.'
    )
    exit 1
}

Write-Host "[INFO] Configuring the WebAssembly build in [$buildDir]." -ForegroundColor Green
emcmake cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release `
    -DMOKU_BUILD_WASM=ON `
    -DMOKU_BUILD_ENGINE=OFF `
    -DMOKU_BUILD_TESTS=OFF `
    -DMOKU_BUILD_PYTHON=OFF `
    -DMOKU_BUILD_NNUE=OFF
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    [Console]::Error.WriteLine(
        "[ERROR] WebAssembly CMake configuration failed (exit code $exitCode)."
    )
    exit $exitCode
}

Write-Host "[INFO] Building the extended WebAssembly engine." -ForegroundColor Green
cmake --build $buildDir --config Release --target extended_moku_wasm
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    [Console]::Error.WriteLine(
        "[ERROR] WebAssembly build failed (exit code $exitCode)."
    )
    exit $exitCode
}

Write-Host "[INFO] WebAssembly build completed successfully in [$buildDir]." -ForegroundColor Green
