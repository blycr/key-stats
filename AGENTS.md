# AGENTS.md

High-signal guidance for OpenCode working in this repo.

## Build & Run

- **Dev mode:** `wails dev` (hot-reload frontend + Go backend)
- **Production build:** `scripts\build.ps1` (PowerShell). Do not run `wails build` directly for releases.
- **Prerequisites:** Go 1.25+, Bun, Wails CLI (`go install github.com/wailsapp/wails/v2/cmd/wails@latest`), Windows 10/11.
- **No tests exist.** There is no test runner to invoke.

## Architecture

Wails v2 desktop app. Go backend + Svelte 4 frontend (Vite, Tailwind). Frameless window with Mica backdrop.

- **Entry:** `main.go` embeds `frontend/dist` and binds `pkg/app/app.go`
- **Hook thread:** `internal/service/keyboard.go` runs a Win32 `WH_KEYBOARD_LL` hook on a dedicated goroutine with its own message pump.
- **Data flow:** Hook callback → buffered channel (cap 4096) → batch writer goroutine → SQLite (WAL). Real-time events go through a separate `rtChan` (cap 256) → `rtEmitter` goroutine → `runtime.EventsEmit`.
- **Frontend polling:** Frontend calls `GetTodayStats()` every 500 ms via `setInterval`.

## Frontend Gotchas

- **Frameless drag swallows clicks:** The title bar `on:mousedown` calls `StartDrag()` (Win32 `SendMessage(WM_NCLBUTTONDOWN)`). This blocks and swallows all child click events. Guard with `if (!e.target.closest('button'))` before calling `StartDrag()`.
- **Never use `document.addEventListener('click', ...)` or Wails `EventsOn`:** Svelte 4 uses event delegation (single document-level listener). Adding your own document-level listeners conflicts with Svelte's event processing and makes buttons unresponsive. Use Svelte `on:click` exclusively. For click-outside, attach `on:click` to a parent element and check `e.target.closest(...)` (see `handleMainClick` in `App.svelte`).
- **Be extremely cautious with `EventsOn` in production:** An attempt to replace the 500 ms polling loop with Wails `EventsOn('key-pressed')` real-time events introduced IPC-level instability. The frontend froze after receiving the first event. The root cause was never fully isolated (possibly contention between `EventsEmit` and Go binding calls inside the same callback). Polling is simple, predictable, and works. Do not switch to event-driven updates without extensive testing on a branch.

## Backend Gotchas

- **Never emit from the hook callback:** `hookProc` runs on a Windows callback thread. Never call `runtime.EventsEmit` or do blocking I/O there. Use non-blocking sends to `rtChan`; the `rtEmitter` goroutine handles emission.
- **Message pump MUST use blocking `GetMessageW`:** `WH_KEYBOARD_LL` hook callbacks are dispatched by Windows through the message pump thread. Replacing `GetMessageW` with `PeekMessageW` + `Sleep` causes non-deterministic failures because Go's scheduler may migrate the goroutine to a different OS thread during `time.Sleep`, and Windows will no longer dispatch hook callbacks to the correct thread. The original `GetMessageW` blocking loop is correct — Go does not migrate goroutines while they are in a blocking syscall, so it effectively pins the thread even without explicit `runtime.LockOSThread()`.
- **Do not add `runtime.LockOSThread()` to the hook message pump without thorough testing:** An attempt to add explicit `LockOSThread()` followed by `PostThreadMessage(WM_QUIT)` wake-up caused the app to freeze after the first keystroke. The exact root cause was never fully determined, but the lesson is: the message pump is a critical and fragile piece of the architecture. Do not change it on `main` without a dedicated test branch and real-world validation.
- **Tray shutdown order:** On shutdown, call `trayMgr.Quit()` before closing the DB.
- **Do not export unused methods on `App`:** Wails auto-generates TS bindings for every exported method. Exporting something like `Ctx() context.Context` causes a TS 2305 error because `context` does not exist in the frontend models.

## Build Gotchas

- **`frontend/dist` must exist before `wails build -s`:** `wails build -s` skips the Vite build but still embeds `frontend/dist` via `//go:embed`. If `dist` is missing or empty, the final `.exe` crashes silently on startup because `-H windowsgui -s -w` suppresses the console.
- **Delete `.syso` before every build:** `rsrc` generates `rsrc_windows_amd64.syso`. Go auto-picks up `.syso` files. If an old one is present, the linker fails with `too many .rsrc sections`. The build script deletes it at the start, regenerates it before the final `go build`, and deletes it again at the end.
- **Do NOT delete `frontend/wailsjs` during clean:** The frontend Vite build imports `../wailsjs/runtime/runtime.js`. If `wailsjs` is missing, Vite fails. If it is accidentally deleted, run `wails generate module` (requires `frontend/dist` to exist — create an empty dir with `.gitkeep` first).
- **Patch generated bindings after every Wails build:** Wails generates `frontend/wailsjs/go/app/App.js` with `// @ts-check`. This causes TS 7006/7015 errors on `window['go']...` bracket access. Do not try to fix this with `jsconfig.json` — `exclude` does not suppress file-level directives, and `jsconfig.json` triggers a separate `moduleResolution=node10` deprecation error. The correct fix is the `patch:wails` npm script (run automatically by the build script) that replaces `// @ts-check` with `// @ts-nocheck`.
- **Build order:** 1) deep clean (`.syso`, `frontend/dist`, `build/bin`) → 2) `bun install` + `bun run build` → 3) `wails build -s` → 4) `bun run patch:wails` → 5) `rsrc` → 6) `go build` → 7) clean up `.syso`.

## Release Workflow

- GitHub Actions (`.github/workflows/release.yml`) triggers on tag push matching `v*`.
- Bump version in `wails.json`, write release notes to `docs/releases/<tag>.md`, commit, then `git tag vX.Y.Z && git push --tags`.
- CI builds on `windows-latest` and attaches `build/bin/key-stats.exe`.

## Config & Data Locations

| Resource | Path |
|----------|------|
| SQLite DB | `%APPDATA%\key-stats\data.db` |
| `.env` config | Next to executable if `PORTABLE_MODE=1`, else `%APPDATA%\key-stats\.env` |
| Release notes | `docs/releases/v<version>.md` |
