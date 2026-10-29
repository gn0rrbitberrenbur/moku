$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot 'moku'
$buildDir = Join-Path $sourceDir 'build'

Write-Host "[INFO] Running CMake configuration in build directory [$buildDir]." -ForegroundColor Green
cmake -S $sourceDir -B $buildDir -DCMAKE_BUILD_TYPE=Release
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"
    [Console]::Error.WriteLine("${red}[ERROR] CMake configuration failed for [$buildDir] (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] CMake configuration was successful." -ForegroundColor Green
Write-Host "[INFO] Starting build process." -ForegroundColor Green

cmake --build $buildDir --config Release
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    $red = "$([char]27)[31m"
    $reset = "$([char]27)[0m"

    [Console]::Error.WriteLine("${red}[ERROR] Build failed in [$buildDir] (exit code $exitCode).${reset}")
    exit $exitCode
}

Write-Host "[INFO] Build completed successfully in [$buildDir]." -ForegroundColor Green