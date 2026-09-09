param([string]$Compiler = 'g++')
$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path $PSScriptRoot -Parent
Push-Location $projectRoot
try {
    New-Item -ItemType Directory -Path build -Force | Out-Null
    & $Compiler -std=c++14 -O2 -Wall -Wextra -Wpedantic -I include src/cache.cpp src/main.cpp -o build/cache_sim.exe
    if ($LASTEXITCODE -ne 0) { throw 'Simulator compilation failed.' }
    & $Compiler -std=c++14 -O2 -Wall -Wextra -Wpedantic -I include src/cache.cpp tests/cache_tests.cpp -o build/cache_tests.exe
    if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    Write-Output 'Built build/cache_sim.exe and build/cache_tests.exe'
} finally {
    Pop-Location
}
