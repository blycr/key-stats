# BLUEPRINT.md — Windows Keyboard Key Statistics Tool

## 1. Project Structure

```
key-stats/
├── main.go                    # Wails entry point: window config (1280x800, frameless, Mica)
├── app.go                     # App struct, Startup/Shutdown lifecycle, Wails-bound API
├── drag_windows.go            # Win32 native window drag for frameless mode (Windows only)
├── wails.json                 # Wails project config
├── go.mod
├── go.sum
├── build/
│   ├── appicon.png            # 256x256 app icon (keyboard-themed)
│   └── windows/
│       ├── icon.ico           # Multi-size ICO for Windows exe
│       ├── info.json
│       └── wails.exe.manifest
├── frontend/
│   ├── index.html
│   ├── package.json
│   ├── vite.config.js
│   ├── tailwind.config.js     # Tailwind CSS v3 with custom palette
│   ├── postcss.config.js
│   ├── bun.lock
│   └── src/
│       ├── main.js            # Svelte bootstrap
│       ├── App.svelte         # Main layout: top bar, stats panel, heatmap
│       ├── app.css            # Tailwind directives + hidden scrollbars
│       └── components/
│           └── KeyboardMap.svelte   # Responsive QWERTY heatmap
├── internal/
│   ├── db/
│   │   └── sqlite.go          # SQLite init + WAL + batch writer (500 ms)
│   ├── models/
│   │   └── models.go          # Go structs
│   ├── service/
│   │   └── keyboard.go        # Win32 LL hook + message pump
│   └── stats/
│       └── stats.go           # Today summary query + VK→name mapping
└── BLUEPRINT.md
```

## 2. Configuration (`wails.json`)

```json
{
  "$schema": "https://wails.io/schemas/config.v2.json",
  "name": "key-stats",
  "outputfilename": "key-stats",
  "frontend:install": "bun install",
  "frontend:build": "bun run build",
  "frontend:dev:watcher": "bun run dev",
  "frontend:dev:serverUrl": "auto",
  "author": {
    "name": "Developer",
    "email": "developer@example.com"
  },
  "info": {
    "companyName": "KeyStats",
    "productName": "KeyStats",
    "productVersion": "1.0.0",
    "copyright": "Copyright 2026",
    "comments": "Windows Keyboard Statistics Tool"
  }
}
```

## 3. Database Schema

Database file path: `%APPDATA%/key-stats/data.db`

### 3.1 Table: `key_events`

Raw event log. One row per keypress.

```sql
CREATE TABLE IF NOT EXISTS key_events (
    id          INTEGER PRIMARY KEY AUTOINCREMENT,
    key_code    INTEGER NOT NULL,   -- Windows Virtual-Key code (VK_*)
    app_name    TEXT    NOT NULL,   -- Foreground window title at time of press
    timestamp   INTEGER NOT NULL    -- Unix epoch milliseconds
);

CREATE INDEX IF NOT EXISTS idx_key_events_timestamp
    ON key_events (timestamp);

CREATE INDEX IF NOT EXISTS idx_key_events_key_code
    ON key_events (key_code);
```

### 3.2 PRAGMA Settings

- `journal_mode=WAL` — Write-Ahead Logging for better concurrency
- `synchronous=NORMAL` — Balance between safety and performance

## 4. Go Backend — Models

```go
// internal/models/models.go

package models

type KeyEvent struct {
    ID        int64  `json:"id"`
    KeyCode   int    `json:"keyCode"`
    AppName   string `json:"appName"`
    Timestamp int64  `json:"timestamp"` // Unix ms
}

type TodaySummary struct {
    TotalKeys    int          `json:"totalKeys"`
    TopKeys      []KeyCount   `json:"topKeys"`      // top 10
    AppBreakdown []AppCount   `json:"appBreakdown"` // currently empty
}

type KeyCount struct {
    KeyCode int    `json:"keyCode"`
    KeyName string `json:"keyName"` // human-readable, e.g. "A", "Shift", "Space"
    Count   int    `json:"count"`
}

type AppCount struct {
    AppName string `json:"appName"`
    Count   int    `json:"count"`
}
```

## 5. API Contract

All functions are methods on the `App` struct in `app.go` (and `drag_windows.go`). Wails auto-generates JS/TS bindings.

```go
// app.go
package main

import "key-stats/internal/models"

// --- Stats Queries ---

// GetTodayStats returns aggregate stats for the current day.
// Numpad digits are merged with main keyboard digits in the aggregation.
func (a *App) GetTodayStats() (models.TodaySummary, error)

// ToggleLogger is currently a placeholder. Returns true.
func (a *App) ToggleLogger() (bool, error)

// --- Frameless Window Drag ---

// StartDrag triggers native Windows window drag.
// Called from frontend mousedown on the top bar.
func (a *App) StartDrag()
```

### 5.1 Internal Lifecycle Hooks (Not Exposed to Wails JS)

```go
// Startup is called when the app starts. Opens DB, starts logger.
func (a *App) Startup(ctx context.Context)

// Shutdown is called when the app is closing. Stops logger, closes DB.
func (a *App) Shutdown(ctx context.Context)
```

## 6. Keyboard Logger Design

### 6.1 Win32 Hook

- Use `SetWindowsHookExW` with `WH_KEYBOARD_LL` (low-level keyboard hook).
- Requires standard user privileges, no admin rights needed.
- Hook procedure captures: virtual key code (`wParam`), foreground window title via `GetForegroundWindow` + `GetWindowTextW`.
- Runs in its own goroutine with a dedicated Windows message pump (`GetMessageW` loop).
- Events are pushed into a buffered channel (cap 4096) consumed by a batch writer goroutine.

### 6.2 Batch Writer

- Flushes channel to SQLite every **500 ms** or when buffer reaches 256 events (whichever first).
- Uses a prepared statement inside a transaction for throughput.
- On `Close()`, drains remaining channel events before shutting down.

### 6.3 VK Code to Key Name Mapping

Located in `internal/stats/stats.go` (`VKToName`).

| Range / VK | Mapped Name |
|------------|-------------|
| `0x41`–`0x5A` (65-90) | "A"–"Z" |
| `0x30`–`0x39` (48-57) | "0"–"9" (main keyboard) |
| `0x60`–`0x69` (96-105) | "0"–"9" (numpad → merged with main) |
| 32 | "Space" |
| 13 | "Enter" |
| 8 | "Back" |
| 16 | "Shift" |
| 17 | "Ctrl" |
| 18 | "Alt" |
| 9 | "Tab" |
| 27 | "Esc" |
| 20 | "Caps" |
| 91, 92 | "Win" (left & right) |
| 106 | "*" |
| 107 | "+" |
| 109 | "-" |
| 110 | "." |
| 111 | "/" |
| 187 | "=" |
| 189 | "-" |
| 190 | "." |
| 188 | "," |
| 191 | "/" |
| 186 | ";" |
| 192 | "`" |
| 219 | "[" |
| 220 | "\\" |
| 221 | "]" |
| 222 | "'" |
| *Unknown* | "Key" |

## 7. UI Specification

### 7.1 Layout

```
┌─────────────────────────────────────────────────────────┐
│  ⌨ KeyStats              [Today ▾]   [● Live]           │  ← TopBar (draggable)
├──────────────────────────┬──────────────────────────────┤
│                          │                              │
│  TODAY'S STATS           │  KEYBOARD HEATMAP            │
│  ─────────────           │  ────────────────            │
│  Total: 12,847           │  ┌───┬───┬───┬───┬───┐      │
│                          │  │ Q │ W │ E │ R │ T │ ...  │
│  TOP KEYS                │  ├───┼───┼───┼───┼───┤      │
│  ────────                │  │ A │ S │ D │ F │ G │ ...  │
│  1. Space    ████  2,103 │  ├───┼───┼───┼───┼───┤      │
│  2. E        ███   1,024 │  │ Z │ X │ C │ V │ B │ ...  │
│  3. A        ██      891 │  └───┴───┴───┴───┴───┘      │
│  4. Backspace ██     756 │                              │
│  5. Enter    █       654 │  Color intensity = relative  │
│                          │  frequency vs busiest key    │
└──────────────────────────┴──────────────────────────────┘
```

### 7.2 Window Config (`main.go`)

```go
&options.App{
    Title:     "KeyStats",
    Width:     1280,
    Height:    800,
    MinWidth:  1100,
    MinHeight: 650,
    Frameless: true,
    BackgroundColour: &options.RGBA{R: 28, G: 28, B: 30, A: 1},
    Windows: &windows.Options{
        WebviewIsTransparent: true,
        WindowIsTranslucent:  true,
        BackdropType:         windows.Mica,
    },
}
```

### 7.3 Tailwind Configuration (v3)

```js
// tailwind.config.js
export default {
  content: ['./src/**/*.{svelte,js,ts}', './index.html'],
  theme: {
    extend: {
      colors: {
        surface: {
          DEFAULT: '#1C1C1E',
          raised: '#2C2C2E',
          overlay: '#3A3A3C',
        },
        accent: {
          DEFAULT: '#6C63FF',
          hover: '#7F78FF',
          muted: '#6C63FF33',
        },
        text: {
          primary: '#F5F5F7',
          secondary: '#A1A1A6',
          tertiary: '#6E6E73',
        },
        heatmap: {
          low: '#2C2C2E',
          mid: '#6C63FF66',
          high: '#6C63FF',
        },
        success: '#30D158',
        danger: '#FF453A',
      },
      fontFamily: {
        sans: ['"Inter"', 'system-ui', 'sans-serif'],
        mono: ['"JetBrains Mono"', 'monospace'],
      },
      borderRadius: {
        card: '12px',
      },
      boxShadow: {
        card: '0 1px 3px rgba(0,0,0,0.3)',
      },
    },
  },
  plugins: [],
}
```

### 7.4 Global Styles

```css
/* src/app.css */
@tailwind base;
@tailwind components;
@tailwind utilities;

body {
  @apply bg-surface text-text-primary font-sans;
  user-select: none;
  margin: 0;
  overflow: hidden;
}

button, a, input, select, [data-clickable] {
  -webkit-app-region: no-drag;
}
```

Scrollbars are **globally hidden** (`width: 0px`) for a cleaner look while preserving scroll functionality.

### 7.5 Component Specifications

#### `App.svelte`

- Full-screen flex column (`w-screen h-screen`)
- **Top bar**: `h-14`, draggable via `on:mousedown` → `window.go.main.App.StartDrag()`
  - Left: keyboard SVG icon + "KeyStats" title
  - Right: "Today" dropdown, Live/Pause toggle with pulsing green dot
- **Left panel** (`w-[320px]`):
  - Today's total keystrokes count (large mono font)
  - Top 10 keys ranking with proportional accent bars
- **Right panel** (flex-1):
  - Keyboard heatmap card with `KeyboardMap` component
- Background decorative blur glow (`bg-accent/5`, `blur-[120px]`)

#### `KeyboardMap.svelte`

- Renders a full QWERTY layout (5 rows including modifiers)
- **Responsive scaling**: uses `ResizeObserver` on parent container
  - Base width: 720 px
  - `scale = min(1, parentWidth / 720)`
  - Applied via `transform: scale()` with `transform-origin: center center`
- Key cells: `rounded-lg`, `h-9`, variable width per key type
- Heat colors:
  - `count == 0`: `bg-surface-overlay text-text-secondary`
  - ratio < 0.2: `bg-heatmap-low`
  - ratio < 0.6: `bg-heatmap-mid`
  - ratio >= 0.6: `bg-heatmap-high` + glow shadow + bold + scale
- Tooltip on hover: key press count

### 7.6 Data Fetching

```js
// App.svelte polling
const interval = setInterval(() => {
    if (isLive) fetchLiveStats();
}, 500); // matches backend batch writer interval
```

### 7.7 Frameless Window Drag

Because WebView2 + Mica does not reliably support CSS `-webkit-app-region: drag`, the app uses a native Win32 approach:

1. `App.svelte` top bar listens for `mousedown`
2. Calls Go-bound `StartDrag()` from `drag_windows.go`
3. `drag_windows.go` uses `ReleaseCapture` + `SendMessageW(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0)`
4. Windows handles the drag natively

## 8. App Lifecycle & Concurrency

```
┌─────────────┐    Startup()     ┌──────────────┐
│   Wails     │ ───────────────→ │  App struct  │
│   Runtime   │                  │              │
└─────────────┘                  │  - db *sql.DB│
                                 │  - keyboard  │
                                 └──────┬───────┘
                                        │
                    ┌───────────────────┼──────────┐
                    ▼                   ▼          │
            ┌──────────────┐  ┌──────────────┐    │
            │  Logger      │  │  Batch       │    │
            │  Goroutine   │  │  Writer      │    │
            │              │  │  Goroutine   │    │
            │ WH_KEYBOARD  │→ │              │→   │
            │ _LL hook     │  │ 500 ms /     │    │
            │ + msg pump   │  │ 256 events   │    │
            └──────────────┘  └──────────────┘    │
                                        │         │
                                        ▼         │
                                 ┌──────────────┐ │
                                 │  SQLite      │ │
                                 │  (WAL mode)  │ │
                                 └──────────────┘ │
                                        ▲         │
                                        │         │
            ┌───────────────────────────┘         │
            │  Frontend poll (500 ms)             │
            └─────────────────────────────────────┘
```

- **Logger goroutine**: owns the Win32 hook, pushes `KeyEvent` structs into channel.
- **Batch writer goroutine**: drains channel, writes to `key_events`.
- **Shutdown**: stops hook, drains remaining channel buffer, closes DB.

## 9. Dependencies

### Go

| Module | Purpose |
|---|---|
| `github.com/wailsapp/wails/v2` | Desktop framework |
| `modernc.org/sqlite` | Pure-Go SQLite driver |

### Frontend

| Package | Purpose |
|---|---|
| `svelte` | UI framework |
| `tailwindcss` | Utility CSS |
| `vite` | Build tool |
| `@sveltejs/vite-plugin-svelte` | Svelte Vite plugin |
| `autoprefixer` / `postcss` | CSS processing |

## 10. Data Retention

- `key_events`: currently retained indefinitely (no automatic purge implemented).
- Future: add retention policy (e.g. 30 days) and daily purge.
