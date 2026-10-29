$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

Write-Host "[INFO] Configuring build for target extended_moku in [$buildDir]." -ForegroundColor Green
cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release `
    -DMOKU_BUILD_ENGINE=ON `
    -DMOKU_BUILD_TESTS=OFF `
    -DMOKU_BUILD_PYTHON=OFF `
    -DMOKU_BUILD_NNUE=OFF
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] CMake configuration failed (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] Building target extended_moku." -ForegroundColor Green
cmake --build $buildDir --config Release --target extended_moku
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] Build failed for extended_moku (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] extended_moku build completed." -ForegroundColor Green