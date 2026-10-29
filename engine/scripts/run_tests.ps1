$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$sourceDir = Join-Path $repoRoot 'moku'
$buildDir = Join-Path $sourceDir 'build'

Write-Host "[INFO] Running tests in build directory [$buildDir]." -ForegroundColor Green

Push-Location $buildDir
try {
    ctest -C Release --output-on-failure
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) {
        $red = "$([char]27)[31m"
        $reset = "$([char]27)[0m"

        [Console]::Error.WriteLine("${red}[ERROR] Tests failed in [$buildDir] (exit code $exitCode).${reset}")
        exit $exitCode
    }
    Write-Host "[INFO] Tests completed successfully in [$buildDir]." -ForegroundColor Green
} finally {
    Pop-Location
}