# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```cmd
:: Dev mode (hot-reload, uses bun for frontend)
wails dev

:: Production build -> build\bin\key-stats.exe
scripts\build.cmd

:: Or manually (wails doesn't embed icon or propagate ldflags correctly):
wails build -s
go run github.com/akavel/rsrc@latest -ico build\windows\icon.ico -o rsrc_windows_amd64.syso
go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w" -o build\bin\key-stats.exe .

:: Frontend only (rarely needed standalone)
cd frontend && bun run build
```

Prerequisites: Go 1.24+, Bun, Wails CLI (`go install github.com/wailsapp/wails/v2/cmd/wails@latest`), Windows 10/11.

## Project Structure

```
key-stats\
├── main.go                     # Entry point (embed + wails.Run)
├── wails.json                  # Wails config (bun scripts)
├── go.mod / go.sum
├── scripts\
│   └── build.cmd               # Production build script
├── build\
│   ├── appicon.png
│   └── windows\
│       ├── icon.ico            # Windows multi-size icon
│       └── wails.exe.manifest
├── frontend\
│   ├── package.json
│   ├── vite.config.js
│   ├── tailwind.config.js
│   └── src\
│       ├── main.js
│       ├── app.css
│       ├── App.svelte          # Main layout, title bar, menus, modals, polling loop
│       └── components\
│           ├── KeyboardMap.svelte  # QWERTY heatmap with auto-scaling via ResizeObserver
│           ├── Modal.svelte        # Glassmorphism dialog (info/confirm modes)
│           └── SettingsPanel.svelte # Theme, startup toggles, saves via Go config API
├── internal\
│   ├── config\
│   │   └── config.go           # .env-based config + window size persistence
│   ├── db\
│   │   └── sqlite.go           # SQLite init + batch writer goroutine
│   ├── models\
│   │   └── models.go           # KeyEvent, DailyStats, TopKey, AppBreakdown structs
│   ├── service\
│   │   └── keyboard.go         # Win32 WH_KEYBOARD_LL hook + message pump goroutine
│   └── stats\
│       └── stats.go            # VK code -> key name mapping (F1-F24, media keys, etc.)
└── pkg\
    ├── app\
    │   ├── app.go              # App struct: Startup, Shutdown, GetTodayStats, ResetStats, etc.
    │   └── drag_windows.go     # Win32 SendMessage(WM_NCLBUTTONDOWN) for frameless drag
    └── tray\
        └── tray_windows.go     # System tray icon + menu (getlantern/systray)
```

## Architecture

Wails v2 desktop app. Go backend embedded in a WebView2 (Chromium) renderer. Frameless window with Mica backdrop.

**Data flow:**
1. Win32 `WH_KEYBOARD_LL` hook (`internal\service\keyboard.go`) captures keystrokes globally from a dedicated goroutine with its own message pump
2. Events pushed to buffered channel (cap 4096) -> batch writer goroutine flushes to SQLite every 500ms or 256 events
3. Real-time events queued to separate `rtChan` (cap 256) -> `rtEmitter` goroutine calls `runtime.EventsEmit` (never call emit from the hook callback thread)
4. Frontend polls `GetTodayStats()` every 500ms via `setInterval`

**Go -> JS binding:** All exported methods on `pkg\app\App` struct are auto-bound to `window.go.app.App.*` by Wails.

**Frontend component tree:**
- `App.svelte` — layout, title bar, dropdown/context menus, modals, polling loop
- `KeyboardMap.svelte` — QWERTY heatmap with auto-scaling via ResizeObserver
- `Modal.svelte` — glassmorphism dialog (info/confirm modes)
- `SettingsPanel.svelte` — theme, startup toggles, saves via Go config API

## Critical Patterns & Gotchas

**Event handling in frameless window:** The title bar `on:mousedown` calls `StartDrag()` (Win32 `SendMessage(WM_NCLBUTTONDOWN)`). This blocks until drag completes and **swallows all child click events**. Must guard with `if (!e.target.closest('button'))` to let buttons work.

**Do NOT use document-level event listeners:** Svelte 4 uses event delegation (single document-level listener for clicks). Adding `document.addEventListener('click', ...)` or using `EventsOn` from the Wails runtime causes conflicts with Svelte's event processing, making buttons unresponsive. Use Svelte's `on:click` handlers exclusively. For "click outside" patterns, use `on:click` on a parent element with target checking (see `handleMainClick` in App.svelte).

**Windows hook callback safety:** `hookProc` runs on a Windows callback thread. Never call `runtime.EventsEmit` or do blocking operations directly. Use non-blocking channel sends to `rtChan`; the `rtEmitter` goroutine handles emission.

**SQLite:** Uses `modernc.org/sqlite` (pure Go, no CGO). WAL mode enabled. Data at `%APPDATA%\key-stats\data.db`.

**Config:** `.env`-based config via `internal\config`. Supports `PORTABLE_MODE`, `START_MINIMIZED`, `AUTO_START`, `THEME`, `WINDOW_WIDTH`, `WINDOW_HEIGHT`.

**System tray:** `pkg\tray\tray_windows.go` uses `getlantern/systray`. Must call `Quit()` on shutdown before closing DB.

## Release Workflow

GitHub Actions (`.github\workflows\release.yml`) triggers on tag push matching `v*`:

1. **Checkout** -> **Setup Go** (from go.mod) -> **Setup Bun** -> **Install Wails CLI**
2. **Build:** `wails build -s` (frontend + bindings) -> `rsrc` (embed icon as Windows resource) -> `go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w"` (stripped GUI binary)
3. **Release notes:** reads `docs\releases\<tag>.md` if it exists
4. **Publish:** creates GitHub Release via `softprops/action-gh-release@v2`, attaches `build\bin\key-stats.exe`

All steps run on `windows-latest`. Release name format: `KeyStats <tag>`.

To cut a release:
```cmd
:: 1. Bump version in wails.json
:: 2. Write release notes: docs\releases\v1.0.1.md
:: 3. Tag and push
git tag v1.0.1
git push --tags
```

## Config & Data Locations

| Resource | Path |
|----------|------|
| SQLite DB | `%APPDATA%\key-stats\data.db` |
| Window size | `%APPDATA%\key-stats\config.json` |
| .env config | Next to executable (portable mode) or `%APPDATA%\key-stats\.env` |
| Release notes | `docs\releases\v<version>.md` |
