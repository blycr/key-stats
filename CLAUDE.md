# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Dev mode (hot-reload, uses bun for frontend)
wails dev

# Production build → build/bin/key-stats.exe
wails build

# Frontend only (rarely needed standalone)
cd frontend && bun run build
```

Prerequisites: Go 1.25+, Bun, Wails CLI (`go install github.com/wailsapp/wails/v2/cmd/wails@latest`), Windows 10/11.

## Architecture

Wails v2 desktop app. Go backend embedded in a WebView2 (Chromium) renderer. Frameless window with Mica backdrop.

**Data flow:**
1. Win32 `WH_KEYBOARD_LL` hook (`internal/service/keyboard.go`) captures keystrokes globally from a dedicated goroutine with its own message pump
2. Events pushed to buffered channel (cap 4096) → batch writer goroutine flushes to SQLite every 500ms or 256 events
3. Real-time events queued to separate `rtChan` (cap 256) → `rtEmitter` goroutine calls `runtime.EventsEmit` (never call emit from the hook callback thread)
4. Frontend polls `GetTodayStats()` every 500ms via `setInterval`

**Go → JS binding:** All exported methods on `pkg/app/App` struct are auto-bound to `window.go.app.App.*` by Wails.

**Frontend component tree:**
- `App.svelte` — layout, title bar, dropdown/context menus, modals, polling loop
- `KeyboardMap.svelte` — QWERTY heatmap with auto-scaling via ResizeObserver
- `Modal.svelte` — glassmorphism dialog (info/confirm modes)
- `SettingsPanel.svelte` — theme, startup toggles, saves to .env

## Critical Patterns & Gotchas

**Event handling in frameless window:** The title bar `on:mousedown` calls `StartDrag()` (Win32 `SendMessage(WM_NCLBUTTONDOWN)`). This blocks until drag completes and **swallows all child click events**. Must guard with `if (!e.target.closest('button'))` to let buttons work.

**Do NOT use document-level event listeners:** Svelte 4 uses event delegation (single document-level listener for clicks). Adding `document.addEventListener('click', ...)` or using `EventsOn` from the Wails runtime causes conflicts with Svelte's event processing, making buttons unresponsive. Use Svelte's `on:click` handlers exclusively. For "click outside" patterns, use `on:click` on a parent element with target checking (see `handleMainClick` in App.svelte).

**Windows hook callback safety:** `hookProc` runs on a Windows callback thread. Never call `runtime.EventsEmit` or do blocking operations directly. Use non-blocking channel sends to `rtChan`; the `rtEmitter` goroutine handles emission.

**SQLite:** Uses `modernc.org/sqlite` (pure Go, no CGO). WAL mode enabled. Data at `%APPDATA%/key-stats/data.db`.

**Config:** `.env`-based config via `internal/config`. Supports `PORTABLE_MODE`, `START_MINIMIZED`, `AUTO_START`, `THEME`, `WINDOW_WIDTH`, `WINDOW_HEIGHT`.

**System tray:** `pkg/tray/tray_windows.go` uses `getlantern/systray`. Must call `Quit()` on shutdown before closing DB.
