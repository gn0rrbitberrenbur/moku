$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

Write-Host "[INFO] Configuring build for target gomocup_protocol_tests in [$buildDir]." -ForegroundColor Green
cmake -S $repoRoot -B $buildDir -DCMAKE_BUILD_TYPE=Release `
    -DMOKU_BUILD_ENGINE=OFF `
    -DMOKU_BUILD_TESTS=ON `
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

Write-Host "[INFO] Building target gomocup_protocol_tests." -ForegroundColor Green
cmake --build $buildDir --config Release --target gomocup_protocol_tests
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] Build failed for gomocup_protocol_tests (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] gomocup_protocol_tests build completed." -ForegroundColor Green