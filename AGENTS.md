# AGENTS.md

High-signal guidance for coding agents working in this repo.

## Build & Run

- **Production build:** `scripts\build.ps1` (or `scripts\build.ps1 -Clean -RunTests`). Produces `build\bin\key-stats.exe` (`~1.2 MB`, zero external DLL/WebView2 runtime dependencies).
- **Run tests:** `scripts\build.ps1 -RunTests` or run `.\build\bin\tests_core.exe` and `.\build\bin\tests_ui.exe` directly.
- **Visual offscreen check:** `.\build\bin\key-stats.exe --snapshot-dir <dir>` renders Dark/Light/Menu-Blur/Settings-Dropdown PNGs cleanly via WIC without blocking.
- **Prerequisites:** MSYS2 MinGW-w64 (`gcc`, `g++`, `windres`), Windows 10/11 SDK headers (included in MSYS2 MinGW-w64). Zero Node/Bun/Go/WebView2 dependency.

## Architecture

Native Windows desktop app written in **C11 core + minimal zero-STL/zero-exception C++ COM wrapper (`src/ui/d2d_renderer.cpp`)** on top of **Win32 + D3D11 + DXGI SwapChainForComposition (`DXGI_ALPHA_MODE_PREMULTIPLIED`) + DirectComposition + Direct2D 1.1 (`ID2D1DeviceContext`) + DirectWrite + SQLite 3 amalgamation**.

- **Entry & Window (`src/main.c`):** Single-instance mutex (`Local\KeyStatsSingleInstanceMutex`), DPI awareness (`DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2`), zero-border frameless window (`WS_EX_APPWINDOW | WS_EX_NOREDIRECTIONBITMAP`, `WS_POPUP | WS_THICKFRAME` + `WM_NCCALCSIZE = 0` + `WM_NCPAINT = 0` + `SWP_FRAMECHANGED` + `WM_NCHITTEST` caption drag/edge resize + `WM_GETMINMAXINFO` taskbar-safe maximize, `780x540` min logical size supporting 4:3 layouts), explicit DWM border suppression (`DWMWA_BORDER_COLOR = DWMWA_COLOR_NONE`, `DWMWA_SYSTEMBACKDROP_TYPE = DWMSBT_NONE`, zero DWM frame margins), persisted `WINDOW_X`/`WINDOW_Y`/`WINDOW_WIDTH`/`WINDOW_HEIGHT`, event-driven wakeups (`KS_WM_KEYS_DIRTY`, `KS_WM_STATS_DIRTY`).
- **Config (`src/core/config.*`):** `.env` loader/saver with `SRWLOCK`, portable mode detection (executable dir vs `%APPDATA%\key-stats`), `color_scheme` (`indigo`/`ocean`/`emerald`/`amber`/`rose`), window rect persistence, `ks_config_manager_reset_defaults`, and `HKCU\...\Run` startup registration.
- **Database (`third_party/sqlite/sqlite3.*`, `src/core/db.*`):** Official SQLite amalgamation compiled into cached `build\obj\sqlite3.o` (`WAL` + `synchronous=NORMAL` + `busy_timeout=5000`), lock-free-friendly 4096-slot ring buffer + `CONDITION_VARIABLE` background writer thread, 100-row chunked multi-row `INSERT` + 15-minute bucket rollups (`key_stats_buckets`, `app_stats_buckets`), historical backfill, and monotonic atomic `version()`.
- **Keyboard Hook (`src/core/keyboard.*`):** Dedicated `WH_KEYBOARD_LL` thread with `GetMessageW` message pump, `HWND`-cached `GetWindowTextW` foreground title lookup, atomic 256-key bitmask (`active_mask[4]`), `180ms` minimum visual key flash, `GetAsyncKeyState` stuck-key recovery, and coalesced `KS_WM_KEYS_DIRTY` UI wakeup.
- **Stats (`src/core/stats.*`):** Continuous cumulative aggregation across all recorded SQLite data via 15-min rollup buckets with raw `key_events` fallback, `KsStatsCache` keyed by `db_version`.
- **Tray (`src/core/tray.*`):** Dedicated thread `Shell_NotifyIconW` tray icon (`IDI_APP_ICON`) + bilingual popup menu (`Show Window`/`显示主界面`, `Pause Recording`/`暂停监听`, `Launch on Startup`/`开机自启`, `Reset Layout & Settings`/`重置布局与设置`, `Quit`/`退出`).
- **UI & Renderer (`src/ui/`):** Pure-C renderer facade (`d2d_renderer.h`) over DirectComposition/Direct2D 1.1 (`Present(0, 0)`) + `CLSID_D2D1GaussianBlur` glass panels (`view_main.c`, `view_keyboard.c`, `view_settings.c`, `widgets.c`, `theme.c`, `i18n.c`). Features 5 color schemes, vertically centered emoji-free typography (including top-right status pill), 4-item `⋮` menu, two-step red confirmation in Settings to clear stats, and virtualized system font dropdown with a borderless themed Win32 `EDIT` child window for native Chinese IME composition.

## Strict Engineering Rules

- **Keep C11 pure everywhere outside `src/ui/d2d_renderer.cpp`:** All business logic, SQLite access, keyboard hooks, tray, themes, layouts, and views compile with `-std=c11 -O2 -Wall -Wextra -Werror`. Only `src/ui/d2d_renderer.cpp` uses C++ (`-std=c++17 -fno-exceptions -fno-rtti`) cleanly for COM interfaces.
- **Zero-blocking hook callback:** `LowLevelKeyboardProc` must NEVER block on SQLite, disk I/O, or heap allocation; push into the fixed 4096-slot ring buffer and update atomics only.
- **Shutdown ordering:** Always stop in order `ks_tray_stop` -> `ks_keyboard_stop` -> `ks_view_main_destroy` -> `ks_d2d_destroy` -> `ks_db_close` before exiting.
- **Render before ShowWindow:** Render the initial frame (`ks_view_main_render`) before `ShowWindow(hwnd, SW_SHOW)` so startup is 100% flicker-free.

## Config & Data Locations

| Resource | Path |
|----------|------|
| SQLite DB | `%APPDATA%\key-stats\data.db` (or next to exe in portable mode) |
| `.env` config | Next to executable if `.env` exists there without `PORTABLE_MODE=false`, else `%APPDATA%\key-stats\.env` |
| Release notes | `docs/releases/v<version>.md` |
