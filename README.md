# KeyStats

Real-time keyboard usage tracker for Windows. Beautiful, minimal, and stays out of your way.

![KeyStats Screenshot](docs/screenshot.jpg)

## Features

- **Global keystroke capture** — low-level Windows hook (`WH_KEYBOARD_LL`), works across all apps
- **Live dashboard** — today's total, top 10 keys ranking, interactive QWERTY heatmap
- **System tray** — minimize to tray, show/quit from tray menu
- **Elegant menus** — `⋯` dropdown + top-bar right-click context menu
- **Persistent window size** — remembers your last resized dimensions
- **Custom modal dialogs** — dark glassmorphism alerts that match the app theme
- **Reset stats** — one-click clear all records with confirmation
- **Comprehensive key mapping** — F1-F24, arrows, media keys, Fn, L/R modifiers, and more
- **Zero-config storage** — SQLite with WAL mode at `%APPDATA%/key-stats/data.db`

## Tech Stack

| Layer | Technology |
|-------|------------|
| Desktop framework | Wails v2 |
| Backend | Go 1.25, modernc.org/sqlite |
| Frontend | Svelte 4, Vite 5 |
| Styling | Tailwind CSS 3 |
| Package manager | Bun |
| System tray | getlantern/systray |

## Project Structure

```
key-stats/
├── main.go                    # Entry point (embed + wails.Run)
├── wails.json                 # Wails config (bun scripts)
├── go.mod / go.sum
├── scripts/
│   └── build.cmd              # Production build script
├── build/
│   ├── appicon.png
│   └── windows/
│       ├── icon.ico           # Windows multi-size icon
│       └── wails.exe.manifest
├── frontend/
│   ├── package.json
│   ├── vite.config.js
│   ├── tailwind.config.js
│   └── src/
│       ├── App.svelte         # Main layout, menus, modals
│       ├── app.css
│       └── components/
│           ├── KeyboardMap.svelte
│           └── Modal.svelte   # Custom glassmorphism dialogs
├── internal/
│   ├── config/
│   │   └── config.go          # Window size persistence
│   ├── db/
│   │   └── sqlite.go          # SQLite + batch writer
│   ├── models/
│   │   └── models.go
│   ├── service/
│   │   └── keyboard.go        # Win32 LL hook
│   └── stats/
│       └── stats.go           # VK code → key name mapping
└── pkg/
    ├── app/
    │   ├── app.go             # App struct, lifecycle, API
    │   └── drag_windows.go    # Native window drag
    └── tray/
        └── tray_windows.go    # System tray icon + menu
```

## Prerequisites

- Go 1.24+
- [Bun](https://bun.sh/)
- Wails CLI: `go install github.com/wailsapp/wails/v2/cmd/wails@latest`
- Windows 10/11

## Development

```cmd
:: Run in dev mode (hot-reload)
wails dev

:: Build production binary
scripts\build.cmd

:: Or manually:
wails build -s
go run github.com/akavel/rsrc@latest -ico build\windows\icon.ico -o rsrc_windows_amd64.syso
go build -tags "desktop,production" -trimpath -ldflags="-H windowsgui -s -w" -o build\bin\key-stats.exe .
```

## Release

Releases are built automatically by GitHub Actions when a version tag is pushed.

```cmd
:: 1. Bump version in wails.json
:: 2. Write release notes: docs\releases\v1.0.1.md
:: 3. Tag and push
git tag v1.0.1
git push --tags
```

The CI workflow compiles `key-stats.exe` and publishes it on the [Releases](https://github.com/blycr/key-stats/releases) page.

---

The `wails.json` is already configured to use `bun`:

```json
{
  "frontend:install": "bun install",
  "frontend:build": "bun run build",
  "frontend:dev:watcher": "bun run dev"
}
```

## Build Output

```
build/bin/key-stats.exe
```

## Data Storage

SQLite at `%APPDATA%/key-stats/data.db` with WAL mode enabled.

Schema auto-creates on first run:

```sql
CREATE TABLE key_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    key_code    INTEGER NOT NULL,   -- Windows VK code
    app_name    TEXT    NOT NULL,   -- foreground window title
    timestamp   INTEGER NOT NULL    -- Unix epoch ms
);
```

Config file (window size) at `%APPDATA%/key-stats/config.json`.

## Architecture

```
Win32 Hook (goroutine)
    ↓
Event Channel (buffered, cap 4096)
    ↓
Batch Writer (goroutine) — 500 ms / 256 events → SQLite (WAL)
    ↑
Wails Frontend ←—— 500 ms poll ——→ Svelte + Tailwind
```

## License

Private project.
