# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

### Dev mode
```cmd
:: Hot-reload frontend + Go backend
wails dev
```

### Production build
```cmd
:: Full clean + rebuild -> build\bin\key-stats.exe
:: Steps: deep clean -> bun install -> vite build -> wails bindings -> TS patch -> rsrc -> go build
scripts\build.ps1
```

### Manual steps (only if debugging the build itself)
```cmd
:: 1. Clean
if exist rsrc_windows_amd64.syso del rsrc_windows_amd64.syso
if exist frontend\dist rmdir /s /q frontend\dist
if exist frontend\wailsjs rmdir /s /q frontend\wailsjs
if exist build\bin rmdir /s /q build\bin

:: 2. Frontend
cd frontend && bun install && bun run build && cd ..

:: 3. Wails bindings + intermediate compile
wails build -s

:: 4. Patch generated TS bindings
cd frontend && bun run patch:wails && cd ..

:: 5. Icon resource
if exist rsrc_windows_amd64.syso del rsrc_windows_amd64.syso
go run github.com/akavel/rsrc@latest -ico build\windows\icon.ico -o rsrc_windows_amd64.syso

:: 6. Final stripped binary
go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w" -o build\bin\key-stats.exe .
```

Prerequisites: Go 1.24+, Bun, Wails CLI (`go install github.com/wailsapp/wails/v2/cmd/wails@latest`), Windows 10/11.

## Project Structure

```
key-stats\
├── main.go                     # Entry point (embed + wails.Run)
├── wails.json                  # Wails config (bun scripts)
├── go.mod / go.sum
├── scripts\
│   └── build.ps1               # Production build script (PowerShell)
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

**Config:** `.env`-based config via `internal\config`. Supports `PORTABLE_MODE`, `START_MINIMIZED`, `AUTO_START`, `THEME`, `FONT_FAMILY`, `WINDOW_WIDTH`, `WINDOW_HEIGHT`.

**System tray:** `pkg\tray\tray_windows.go` uses `getlantern/systray`. Must call `Quit()` on shutdown before closing DB.

## Build Gotchas

**`frontend/dist` must exist before `wails build -s`:** `wails build -s` skips the frontend Vite build but still embeds `frontend/dist` into the Go binary via `//go:embed`. If `dist` is empty (e.g., only `.gitkeep`), the produced EXE crashes immediately on startup with no visible error because `-H windowsgui -s -w` suppresses the console. The build script therefore runs `bun run build` manually before `wails build -s`.

**`.syso` file must be deleted before every build:** `rsrc` generates `rsrc_windows_amd64.syso`. Go automatically picks up `.syso` files in the package directory. If an old `.syso` is present, the linker fails with `too many .rsrc sections`. The build script deletes it at the start, regenerates it before the final `go build`, and deletes it again at the end.

**Never export unused Go methods bound to Wails JS:** `pkg/app/app.go` previously exported `Ctx() context.Context`. Wails auto-generated `frontend/wailsjs/go/app/App.d.ts` with `import {context} from '../models'`, causing a TS 2305 error because no `context` type exists in the frontend models. Removed `Ctx()` from `App` struct and from the generated `.d.ts`.

**Generated `wailsjs/go/app/App.js` has `// @ts-check`:** Wails generates this file with `// @ts-check` on every build, which makes VS Code report TS 7006/7015 errors (implicit any) on `window['go']...` bracket access. Do NOT try to fix this with `jsconfig.json` — the `exclude` option does not suppress file-level `// @ts-check` directives, and adding `jsconfig.json` triggers a separate `moduleResolution=node10` deprecation error. The correct fix is a post-build patch (`patch:wails` in `frontend/package.json`) that replaces `// @ts-check` with `// @ts-nocheck` after every binding generation. The build script runs this patch automatically.

**Do NOT delete `frontend/wailsjs` during clean:** The frontend build (`vite build`) imports `../wailsjs/runtime/runtime.js`. If `wailsjs` is missing, Vite fails with "Could not resolve '../wailsjs/runtime/runtime.js'". The build script therefore only cleans `frontend/dist` and `build/bin`, not `wailsjs`. If `wailsjs` is ever accidentally deleted, run `wails generate module` (requires `frontend/dist` to exist — create an empty dir with `.gitkeep` first).

**Build order matters:**
1. Deep clean (`.syso`, `frontend/dist`, `build/bin`)
2. `bun install` + `bun run build` (frontend)
3. `wails build -s` (bindings + intermediate compile)
4. `bun run patch:wails` (disable TS strict check)
5. `rsrc` (icon `.syso`)
6. `go build` (final stripped binary)
7. Clean up temp `.syso`

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
