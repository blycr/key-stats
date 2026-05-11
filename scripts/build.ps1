#Requires -Version 5.1
<#
.SYNOPSIS
    Production build script for key-stats.
.DESCRIPTION
    Full clean + rebuild flow:
    1. Deep clean stale artifacts
    2. Regenerate wailsjs bindings if missing
    3. Install frontend dependencies
    4. Build frontend (Vite)
    5. Generate Wails bindings + intermediate compile
    6. Patch generated TS bindings (@ts-check -> @ts-nocheck)
    7. Generate Windows icon resource (.syso)
    8. Final Go compile (stripped, GUI subsystem)
    9. Clean up temporary .syso
#>

$ErrorActionPreference = "Continue"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

Write-Host "[0/6] Deep cleaning stale artifacts..."
Remove-Item -Recurse -Force "rsrc_windows_amd64.syso" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "frontend/dist" -ErrorAction SilentlyContinue
Remove-Item -Recurse -Force "build/bin" -ErrorAction SilentlyContinue
go clean -cache

# If wailsjs was deleted in a previous run, regenerate it so vite build can resolve imports.
if (-not (Test-Path "frontend/wailsjs")) {
    Write-Host "[0a/6] Regenerating wailsjs bindings (needed by frontend build)..."
    New-Item -ItemType Directory -Path "frontend/dist" -Force | Out-Null
    New-Item -ItemType File -Path "frontend/dist/.gitkeep" -Force | Out-Null
    wails generate module
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "[1/6] Installing frontend dependencies..."
Set-Location frontend
bun install
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[2/6] Building frontend (Vite production bundle)..."
bun run build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Set-Location ..

Write-Host "[3/6] Generating Wails bindings and intermediate compile..."
wails build -s
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[4/6] Patching generated TS bindings (disable strict check in App.js)..."
Set-Location frontend
bun run patch:wails
Set-Location ..
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[5/6] Generating Windows icon resource..."
Remove-Item -Force "rsrc_windows_amd64.syso" -ErrorAction SilentlyContinue
go run github.com/akavel/rsrc@latest -ico build/windows/icon.ico -o rsrc_windows_amd64.syso
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[6/6] Final Go compile (stripped, GUI subsystem, icon embedded)..."
go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w" -o build/bin/key-stats.exe .
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# Clean up temporary .syso so it doesn't leak into the next build
Remove-Item -Force "rsrc_windows_amd64.syso" -ErrorAction SilentlyContinue

Write-Host ""
Write-Host "=== Build complete ===" -ForegroundColor Green
Get-Item build/bin/key-stats.exe | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
