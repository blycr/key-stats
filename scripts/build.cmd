@echo off
REM Build script: frontend + bindings via Wails, optimized compile via go build
cd /d "%~dp0.."

REM Step 1: Build frontend and generate Wails bindings
wails build -s
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Step 2: Generate Windows resource file for icon embedding
go run github.com/akavel/rsrc@latest -ico build\windows\icon.ico -o rsrc_windows_amd64.syso
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

REM Step 3: Compile stripped binary with GUI subsystem
go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w" -o build\bin\key-stats.exe .
if %ERRORLEVEL% neq 0 exit /b %ERRORLEVEL%

echo Built: key-stats.exe
dir build\bin\key-stats.exe
