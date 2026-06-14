# Setup script for podofo-font-tools
param(
    [string]$VcpkgPath = "C:\dev\vcpkg"
)

Write-Host "=== PoDoFo Font Tools Setup ===" -ForegroundColor Cyan

# Check if vcpkg exists
if (-not (Test-Path "$VcpkgPath\vcpkg.exe")) {
    Write-Host "vcpkg not found at $VcpkgPath" -ForegroundColor Yellow
    Write-Host "Installing vcpkg to $VcpkgPath..." -ForegroundColor Green

    git clone https://github.com/microsoft/vcpkg.git $VcpkgPath
    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to clone vcpkg" -ForegroundColor Red
        exit 1
    }

    Push-Location $VcpkgPath
    .\bootstrap-vcpkg.bat
    Pop-Location

    if ($LASTEXITCODE -ne 0) {
        Write-Host "Failed to bootstrap vcpkg" -ForegroundColor Red
        exit 1
    }
    Write-Host "vcpkg installed successfully" -ForegroundColor Green
}
else {
    Write-Host "vcpkg found at $VcpkgPath" -ForegroundColor Green
}

# Set env var and verify
$env:VCPKG_ROOT = $VcpkgPath
Write-Host "VCPKG_ROOT = $env:VCPKG_ROOT" -ForegroundColor Green

Write-Host ""
Write-Host "Setup complete! Now run:" -ForegroundColor Cyan
Write-Host "  cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=`"$VcpkgPath\scripts\buildsystems\vcpkg.cmake`"" -ForegroundColor Yellow
Write-Host "  cmake --build build --config Release" -ForegroundColor Yellow
