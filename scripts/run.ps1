# Run script for podofo-font-tools
param(
    [string]$Type = "all",
    [int]$MaxFonts = 10,
    [string]$OutputDir = "",
    [string]$Config = "Release"
)

$ProjectRoot = Split-Path -Parent $PSScriptRoot
$Exe = "$ProjectRoot\build\$Config\podofo-font-classifier.exe"

if (-not (Test-Path $Exe)) {
    Write-Host "ERROR: Executable not found at $Exe" -ForegroundColor Red
    Write-Host "Run build.ps1 first" -ForegroundColor Yellow
    exit 1
}

if ([string]::IsNullOrEmpty($OutputDir)) {
    $OutputDir = "$ProjectRoot\output"
}

Write-Host "=== Running PoDoFo Font Classifier ===" -ForegroundColor Cyan
Write-Host "Type filter: $Type" -ForegroundColor Gray
Write-Host "Max fonts:   $MaxFonts" -ForegroundColor Gray
Write-Host "Output dir:  $OutputDir" -ForegroundColor Gray
Write-Host ""

# Ensure output directory exists
New-Item -ItemType Directory -Force -Path $OutputDir | Out-Null

# Run
if ($Type -eq "all") {
    & $Exe --max $MaxFonts $OutputDir
}
else {
    & $Exe --type $Type --max $MaxFonts $OutputDir
}

if ($LASTEXITCODE -ne 0) {
    Write-Host "Run failed with exit code $LASTEXITCODE" -ForegroundColor Red
}
else {
    Write-Host ""
    Write-Host "Output files:" -ForegroundColor Green
    Get-ChildItem -Recurse -Filter "*.pdf" $OutputDir | Select-Object FullName, Length
}
