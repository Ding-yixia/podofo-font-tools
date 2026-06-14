# Build script for podofo-font-tools
param(
    [string]$VcpkgPath = "C:\dev\vcpkg",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$ProjectRoot = Split-Path -Parent $PSScriptRoot

Write-Host "=== Building PoDoFo Font Tools ===" -ForegroundColor Cyan
Write-Host "Config: $Config" -ForegroundColor Gray

$Toolchain = "$VcpkgPath\scripts\buildsystems\vcpkg.cmake"
if (-not (Test-Path $Toolchain)) {
    Write-Host "ERROR: vcpkg toolchain not found at $Toolchain" -ForegroundColor Red
    Write-Host "Run setup.ps1 first or specify correct VcpkgPath" -ForegroundColor Yellow
    exit 1
}

Write-Host "Configuring CMake..." -ForegroundColor Green
cmake -B "$ProjectRoot\build" -S "$ProjectRoot" `
    -DCMAKE_TOOLCHAIN_FILE="$Toolchain" `
    -DCMAKE_BUILD_TYPE=$Config

if ($LASTEXITCODE -ne 0) {
    Write-Host "CMake configuration failed" -ForegroundColor Red
    exit 1
}

Write-Host "Building..." -ForegroundColor Green
cmake --build "$ProjectRoot\build" --config $Config

if ($LASTEXITCODE -ne 0) {
    Write-Host "Build failed" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "Build complete!" -ForegroundColor Green
Write-Host "Executable: $ProjectRoot\build\$Config\podofo-font-classifier.exe" -ForegroundColor Yellow
