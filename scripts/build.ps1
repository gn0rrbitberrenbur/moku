$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

Write-Host "[INFO] Running full CMake configuration in build directory [$buildDir]." -ForegroundColor Green
cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release `
    -DMOKU_BUILD_ENGINE=ON `
    -DMOKU_BUILD_TESTS=ON `
    -DMOKU_BUILD_PYTHON=ON `
    -DMOKU_BUILD_NNUE=ON
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] CMake configuration failed for [$buildDir] (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] CMake configuration was successful." -ForegroundColor Green
Write-Host "[INFO] Starting full build process." -ForegroundColor Green

cmake --build $buildDir --config Release
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] Full build failed in [$buildDir] (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] Full build completed successfully in [$buildDir]." -ForegroundColor Green