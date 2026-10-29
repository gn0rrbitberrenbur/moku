$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $repoRoot 'build'

if (-not (Test-Path $buildDir))
{
    throw "[ERROR] Build directory not found: $buildDir"
}

Write-Host "[INFO] Running NNUE tests." -ForegroundColor Green
ctest --test-dir $buildDir --output-on-failure -R "^nnue_tests"
$exitCode = $LASTEXITCODE
if ($exitCode -ne 0)
{
    exit $exitCode
}