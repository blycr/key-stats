#Requires -Version 5.1
<#
.SYNOPSIS
    Native C11 + Direct2D/DirectComposition build script for KeyStats.
.DESCRIPTION
    Builds build\bin\key-stats.exe along with unit test binaries tests_core.exe and tests_ui.exe.
    Zero WebView2 dependency; compiles cleanly with MinGW-w64 (gcc / g++ / windres).
#>
param(
    [switch]$Clean,
    [switch]$RunTests
)

$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

if ($Clean) {
    Remove-Item -Recurse -Force "build\obj", "build\bin" -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force -Path "build\obj", "build\bin" | Out-Null

$cflags = @("-std=c11", "-O2", "-Wall", "-Wextra", "-Werror", "-DUNICODE", "-D_UNICODE", "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00", "-Isrc", "-Ithird_party\sqlite")
$cxxflags = @("-std=c++17", "-O2", "-fno-exceptions", "-fno-rtti", "-Wall", "-Wextra", "-Werror", "-DUNICODE", "-D_UNICODE", "-D_WIN32_WINNT=0x0A00", "-DWINVER=0x0A00", "-Isrc")

if (!(Test-Path "build\obj\sqlite3.o") -or ((Get-Item "third_party\sqlite\sqlite3.c").LastWriteTime -gt (Get-Item "build\obj\sqlite3.o").LastWriteTime)) {
    Write-Host "[1/5] Compiling SQLite 3 amalgamation..."
    & gcc -c "third_party\sqlite\sqlite3.c" -o "build\obj\sqlite3.o" -O2 -DSQLITE_THREADSAFE=1 -DSQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "[1/5] Reusing cached build\obj\sqlite3.o..."
}

Write-Host "[2/5] Compiling Win32 resources (manifest + multi-size icon)..."
& windres -Isrc "src\resources\app.rc" -O coff -o "build\obj\app_res.o"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[3/5] Compiling Direct2D / DirectComposition renderer..."
& g++ @cxxflags -c "src\ui\d2d_renderer.cpp" -o "build\obj\d2d_renderer.o"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "[4/5] Compiling C11 core & UI modules..."
$c_sources = @(
    "src\core\win32_util.c",
    "src\core\config.c",
    "src\core\db.c",
    "src\core\stats.c",
    "src\core\keyboard.c",
    "src\core\tray.c",
    "src\ui\theme.c",
    "src\ui\i18n.c",
    "src\ui\widgets.c",
    "src\ui\view_keyboard.c",
    "src\ui\view_settings.c",
    "src\ui\view_main.c"
)

$objs = @("build\obj\sqlite3.o", "build\obj\d2d_renderer.o")
foreach ($src in $c_sources) {
    $name = [System.IO.Path]::GetFileNameWithoutExtension($src)
    $obj = "build\obj\$name.o"
    & gcc @cflags -c $src -o $obj
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    $objs += $obj
}

$win_libs = @(
    "-luser32", "-lgdi32", "-lshell32", "-ladvapi32", "-lole32", "-lcomctl32",
    "-ldwmapi", "-ld3d11", "-ldxgi", "-ld2d1", "-ldwrite", "-ldcomp",
    "-lwindowscodecs", "-luuid", "-lm"
)

Write-Host "[5/5] Linking build\bin\key-stats.exe & unit test suites..."
& gcc @cflags -c "src\main.c" -o "build\obj\main.o"
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& gcc -O2 -s -mwindows -municode -Isrc "build\obj\main.o" @objs "build\obj\app_res.o" -o "build\bin\key-stats.exe" -static-libgcc @win_libs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& gcc @cflags -I. "tests\test_core.c" "build\obj\win32_util.o" "build\obj\config.o" "build\obj\db.o" "build\obj\stats.o" "build\obj\keyboard.o" "build\obj\sqlite3.o" -o "build\bin\tests_core.exe" -luser32 -lshell32 -ladvapi32 -lole32
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

& gcc @cflags -I. "tests\test_ui.c" @objs -o "build\bin\tests_ui.exe" @win_libs
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if ($RunTests) {
    Write-Host ""
    Write-Host "Running unit tests..."
    & ".\build\bin\tests_core.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    & ".\build\bin\tests_ui.exe"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host ""
Write-Host "=== Build complete ===" -ForegroundColor Green
Get-Item "build\bin\key-stats.exe" | Select-Object Name, Length, LastWriteTime | Format-Table -AutoSize
