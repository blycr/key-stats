# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Test

```powershell
# Clean build + compile native key-stats.exe + run all C11 core & UI unit tests
scripts\build.ps1 -Clean -RunTests

# Incremental build (reuses cached sqlite3.o)
scripts\build.ps1 -RunTests

# Export headless Direct2D visual snapshots (Dark, Light, Gaussian Blur Menu, Settings + Font Dropdown)
.\build\bin\key-stats.exe --snapshot-dir .\snapshots
```

Prerequisites: MSYS2 MinGW-w64 (`gcc`, `g++`, `windres`), Windows 10/11. Zero Go, Bun, Node, or WebView2 runtime required.

## Project Structure

```
key-stats\
├── scripts\
│   ├── build.ps1                 # Clean/incremental build + unit tests -> build\bin\key-stats.exe
│   └── Clear-IconCache.ps1       # Helper: clear Windows icon cache
├── build\
│   ├── appicon.png               # Source icon
│   └── windows\
│       └── icon.ico              # Multi-size Windows icon embedded via windres
├── third_party\sqlite\
│   ├── sqlite3.c / sqlite3.h     # Official SQLite 3 amalgamation
├── src\
│   ├── main.c                    # Win32 entry, borderless WS_EX_NOREDIRECTIONBITMAP chrome, DPI/rect persistence, WM_APP wakeups, snapshot CLI
│   ├── core\
│   │   ├── win32_util.h/.c       # UTF-8/UTF-16 conversion, recursive mkdir, single-instance mutex
│   │   ├── config.h/.c           # .env loader/saver, SRWLOCK manager, portable detection, HKCU Run, window rect/scheme reset
│   │   ├── db.h/.c               # SQLite WAL + 4096-slot ring buffer + 15-min rollup buckets
│   │   ├── stats.h/.c            # Continuous cumulative rollup queries, VK name map, db_version snapshot cache
│   │   ├── keyboard.h/.c         # WH_KEYBOARD_LL hook + HWND title cache + 180ms flash bitmask
│   │   └── tray.h/.c             # Dedicated thread Shell_NotifyIconW tray + EN/ZH menu (Show/Pause/AutoStart/Reset/Quit)
│   ├── ui\
│   │   ├── d2d_renderer.h/.cpp   # Zero-STL C++ D3D11 + DirectComposition + Direct2D 1.1 + GaussianBlur
│   │   ├── theme.h/.c            # Dark/Light palettes + 5 color schemes + 0.35s eased transitions + Windows Auto detect
│   │   ├── i18n.h/.c             # Bilingual EN / 中文 dictionaries (emoji-free)
│   │   ├── widgets.h/.c          # Buttons, toggles, progress bars, scrim, number formatting
│   │   ├── view_keyboard.h/.c    # 6-row responsive QWERTY heatmap + side-aware key flash
│   │   ├── view_settings.h/.c    # Settings glass modal, color schemes, virtualized font search (Win32 EDIT IME), 2-step red stats clear
│   │   └── view_main.h/.c        # Titlebar, Top Apps/Keys cards, blurred menus, Status modal
│   └── resources\
│       ├── resource.h / app.rc   # Embedded appicon + PerMonitorV2 manifest
│       └── app.manifest          # Windows PerMonitorV2 DPI + Common Controls v6
└── tests\
    ├── test_runner.h             # Lightweight C11 test harness macros
    ├── test_core.c               # Config/4:3 rect/reset, DB rollup/flush/reset, continuous stats/VK, keyboard bitmask tests
    └── test_ui.c                 # Keyboard geometry/heat, virtual scroll, font filter, theme/scheme, i18n tests
```
