$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

Write-Host "[INFO] Configuring build for target nnue_gen in [$buildDir]." -ForegroundColor Green
cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release `
    -DMOKU_BUILD_ENGINE=OFF `
    -DMOKU_BUILD_TESTS=OFF `
    -DMOKU_BUILD_PYTHON=OFF `
    -DMOKU_BUILD_NNUE=ON
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] CMake configuration failed (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] Building target nnue_gen." -ForegroundColor Green
cmake --build $buildDir --config Release --target nnue_gen
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] Build failed for nnue_gen (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] nnue_gen build completed." -ForegroundColor Green