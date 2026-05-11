# BLUEPRINT.md — Windows Keyboard Key Statistics Tool

## 1. Project Structure

```
key-stats/
├── main.go
├── app.go
├── wails.json
├── go.mod
├── go.sum
├── build/
│   └── appicon.png
├── frontend/
│   ├── index.html
│   ├── package.json
│   ├── svelte.config.js
│   ├── vite.config.js
│   ├── tailwind.config.js   # Tailwind CSS v3
│   ├── postcss.config.js
│   ├── src/
│   │   ├── main.js
│   │   ├── App.svelte
│   │   ├── app.css
│   │   ├── lib/
│   │   │   ├── StatsTable.svelte
│   │   │   ├── TopBar.svelte
│   │   │   ├── KeyHeatmap.svelte
│   │   │   └── DailyChart.svelte
│   │   └── stores/
│   │       └── stats.js
│   └── wailsjs/
│       ├── go/
│       │   └── main/
│       │       ├── App.js        # auto-generated bindings
│       │       └── App.d.ts
│       └── runtime/
│           └── runtime.js
├── internal/
│   ├── db/
│   │   ├── db.go                # SQLite init + migrations
│   │   └── db_test.go
│   ├── models/
│   │   └── models.go            # Go structs matching schema
│   ├── logger/
│   │   ├── logger.go            # Global keyboard hook (Win32)
│   │   └── logger_test.go
│   └── stats/
│       ├── stats.go             # Query/aggregation logic
│       └── stats_test.go
└── BLUEPRINT.md
```

## 2. Configuration Example (`wails.json`)

```json
{
  "$schema": "https://wails.io/schemas/config.v2.json",
  "name": "key-stats",
  "outputfilename": "key-stats",
  "frontend:install": "npm install",
  "frontend:build": "npm run build",
  "frontend:dev:watcher": "npm run dev",
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

### 3.2 Table: `daily_stats`

Materialized daily aggregate. Updated by a cron-like tick every 5 minutes and on app shutdown.

```sql
CREATE TABLE IF NOT EXISTS daily_stats (
    date        TEXT    NOT NULL,   -- 'YYYY-MM-DD'
    key_code    INTEGER NOT NULL,
    total_count INTEGER NOT NULL DEFAULT 0,
    PRIMARY KEY (date, key_code)
);
```

### 3.3 Table: `meta`

App-level key-value store. (Using `camelCase` for keys or general string format).

```sql
CREATE TABLE IF NOT EXISTS meta (
    key   TEXT PRIMARY KEY,
    value TEXT NOT NULL
);
-- Seeded with: ('schemaVersion', '1'), ('loggingEnabled', 'true')
```

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

type DailyStat struct {
    Date       string `json:"date"`       // 'YYYY-MM-DD'
    KeyCode    int    `json:"keyCode"`
    TotalCount int    `json:"totalCount"`
}

type TodaySummary struct {
    TotalKeys   int          `json:"totalKeys"`
    TopKeys     []KeyCount   `json:"topKeys"`     // top 10
    AppBreakdown []AppCount  `json:"appBreakdown"` // top 5 apps
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

type WeeklyTrend struct {
    Dates  []string `json:"dates"`  // last 7 dates
    Counts []int    `json:"counts"` // total per day
}
```

## 5. API Contract

All functions are methods on the `App` struct in `app.go`. Wails auto-generates JS/TS bindings. Note that Wails requires backend functions to return `error` as the second return value (or sole return value) to map to Promise rejections in JavaScript.

```go
// app.go
package main

import "key-stats/internal/models"

// --- Stats Queries ---

// GetTodayStats returns aggregate stats for the current day.
func (a *App) GetTodayStats() (models.TodaySummary, error)

// GetDailyStats returns per-key counts for a given date string 'YYYY-MM-DD'.
func (a *App) GetDailyStats(date string) ([]models.DailyStat, error)

// GetWeeklyTrend returns total keypress count per day for the last 7 days.
func (a *App) GetWeeklyTrend() (models.WeeklyTrend, error)

// GetTopAppsToday returns the top N apps by keypress count today.
func (a *App) GetTopAppsToday(limit int) ([]models.AppCount, error)

// --- Control ---

// ToggleLogger enables or disables the keyboard hook. Returns new state.
func (a *App) ToggleLogger() (bool, error)

// IsLogging returns current logging state.
func (a *App) IsLogging() (bool, error)

// FlushStats manually triggers daily_stats materialization.
func (a *App) FlushStats() error

// --- Maintenance ---

// PurgeOldData deletes raw key_events older than N days. Returns deleted count.
func (a *App) PurgeOldData(days int) (int64, error)

// ExportCSV exports key_events for a date range to a CSV file. Returns file path.
func (a *App) ExportCSV(startDate, endDate string) (string, error)
```

### 5.1 Internal Lifecycle Hooks (Not Exposed to Wails JS)

```go
// Startup is called when the app starts. Opens DB, starts logger.
func (a *App) Startup(ctx context.Context)

// Shutdown is called when the app is closing. Flushes daily_stats, stops logger.
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

- Flushes channel to SQLite every 2 seconds or when buffer reaches 256 events (whichever first).
- Uses a single `INSERT ... VALUES (...),(...),(...)` statement or transaction for throughput.
- On flush, also updates the in-memory `daily_stats` accumulator.

### 6.3 VK Code to Key Name Mapping

- Maintain a static `map[int]string` covering VK codes `0x08`–`0xFE`.
- Special keys: `VK_SHIFT` → "Shift", `VK_CONTROL` → "Ctrl", `VK_MENU` → "Alt", `VK_SPACE` → "Space", `VK_RETURN` → "Enter", `VK_BACK` → "Backspace", `VK_TAB` → "Tab", `VK_ESCAPE` → "Esc".
- Letters `0x41`–`0x5A` → "A"–"Z", digits `0x30`–`0x39` → "0"–"9".
- Unknown codes → "VK_0xNN".

## 7. UI Specification

### 7.1 Layout (ASCII Mockup)

```
┌─────────────────────────────────────────────────────────┐
│  ⌨ KeyStats              [Today ▾]   [● Live] [Export]  │  ← TopBar
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
│  5. Shift    █       654 │  Color intensity = relative  │
│                          │  frequency vs busiest key    │
│  TOP APPS                │                              │
│  ────────                ├──────────────────────────────┤
│  1. Chrome      5,230    │                              │
│  2. VS Code     3,100    │  WEEKLY TREND                │
│  3. Terminal    1,200    │  ────────────                │
│                          │  14k│        ╭──╮            │
│                          │  12k│   ╭──╮ │  │            │
│                          │  10k│──╯    ╰╯  ╰──          │
│                          │     └──┬──┬──┬──┬──┬──┬──    │
│                          │       Mo Tu We Th Fr Sa Su    │
│                          │                              │
└──────────────────────────┴──────────────────────────────┘
```

### 7.2 Tailwind Configuration (v3)

```js
// tailwind.config.js
export default {
  content: ['./src/**/*.{svelte,js,ts}'],
  theme: {
    extend: {
      colors: {
        // Raycast-inspired dark palette
        surface: {
          DEFAULT: '#1C1C1E',
          raised: '#2C2C2E',
          overlay: '#3A3A3C',
        },
        accent: {
          DEFAULT: '#6C63FF',   // primary violet
          hover: '#7F78FF',
          muted: '#6C63FF33',   // 20% opacity
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

### 7.3 Global Styles

```css
/* src/app.css */
@tailwind base;
@tailwind components;
@tailwind utilities;

body {
  @apply bg-surface text-text-primary font-sans;
  -webkit-app-region: drag;       /* Wails window dragging */
  user-select: none;
}

/* Interactive elements must opt out of drag */
button, a, input, select, [data-clickable] {
  -webkit-app-region: no-drag;
}

/* Scrollbar styling */
::-webkit-scrollbar { width: 6px; }
::-webkit-scrollbar-track { background: transparent; }
::-webkit-scrollbar-thumb { background: #3A3A3C; border-radius: 3px; }
::-webkit-scrollbar-thumb:hover { background: #4A4A4C; }
```

### 7.4 Component Specifications

#### `TopBar.svelte`

- Fixed height: `h-12`
- Left: app title with keyboard icon, `text-primary font-semibold`
- Center: date picker dropdown (today / yesterday / custom range)
- Right: `● Live` toggle button (green when active, gray when paused), `Export` button
- Background: `bg-surface-raised`, bottom border: `border-b border-surface-overlay`

#### `StatsTable.svelte`

- Props: `data: KeyCount[]`, `title: string`
- Renders a ranked list with:
  - Rank number (`text-text-tertiary`)
  - Key name in monospace (`font-mono bg-surface-overlay px-2 py-0.5 rounded`)
  - Horizontal bar (width proportional to count / max count, `bg-accent`)
  - Count value (`text-text-secondary font-mono`)
- Animate bar width on data change (CSS transition, 300ms ease)

#### `KeyHeatmap.svelte`

- Renders a QWERTY keyboard layout as a CSS Grid
- Three rows: digits (10 keys), QWERTY row (10 keys), ASDF row (9 keys), ZXCV row (7 keys) + modifier keys
- Each key cell: `w-10 h-10 rounded-lg flex items-center justify-center text-xs font-mono`
- Background color interpolated from `heatmap.low` → `heatmap.mid` → `heatmap.high` based on relative count
- Tooltip on hover: key name + exact count

#### `DailyChart.svelte`

- SVG-based 7-day line chart
- Line: `stroke-accent`, fill below line: `fill-accent-muted`
- X-axis: day abbreviations (`text-text-tertiary text-xs`)
- Y-axis: auto-scaled count with k suffix
- Responsive: uses `viewBox` for scaling

### 7.5 Reactive Store

```js
// src/stores/stats.js
import { writable } from 'svelte/store';

export const todayStats = writable(null);
export const weeklyTrend = writable(null);
export const isLogging = writable(true);
export const selectedDate = writable(new Date().toISOString().slice(0, 10));

// Polling: refresh data every 3 seconds while window is focused
let interval;
export function startPolling(getTodayStats, getWeeklyTrend) {
  const refresh = async () => {
    try {
        todayStats.set(await getTodayStats());
        weeklyTrend.set(await getWeeklyTrend());
    } catch(err) {
        console.error("Failed to fetch stats", err);
    }
  };
  refresh();
  interval = setInterval(refresh, 3000);
}

export function stopPolling() {
  clearInterval(interval);
}
```

## 8. App Lifecycle & Concurrency

```
┌─────────────┐    Startup()     ┌──────────────┐
│   Wails     │ ───────────────→ │  App struct  │
│   Runtime   │                  │              │
└─────────────┘                  │  - db *sql.DB│
                                 │  - logger    │
                                 │  - ch chan   │
                                 └──────┬───────┘
                                        │
                    ┌───────────────────┼───────────────────┐
                    ▼                   ▼                   ▼
            ┌──────────────┐  ┌──────────────┐  ┌──────────────┐
            │  Logger      │  │  Batch       │  │  Stats       │
            │  Goroutine   │  │  Writer      │  │  Ticker      │
            │              │  │  Goroutine   │  │  (5 min)     │
            │ WH_KEYBOARD  │  │              │  │              │
            │ _LL hook     │→ │ buffered     │→ │ materialize  │
            │ + msg pump   │  │ INSERT       │  │ daily_stats  │
            └──────────────┘  └──────────────┘  └──────────────┘
                                        │
                                        ▼
                                 ┌──────────────┐
                                 │  SQLite      │
                                 │  (WAL mode)  │
                                 └──────────────┘
```

- **Logger goroutine**: owns the Win32 hook, pushes `KeyEvent` structs into channel.
- **Batch writer goroutine**: drains channel, writes to `key_events`, updates in-memory daily accumulator.
- **Stats ticker goroutine**: every 5 minutes, flushes accumulator to `daily_stats` table.
- **Shutdown**: flushes remaining channel buffer, materializes final `daily_stats`, closes DB.

## 9. Dependencies

### Go

| Module | Purpose |
|---|---|
| `github.com/wailsapp/wails/v2` | Desktop framework |
| `modernc.org/sqlite` | Pure-Go SQLite driver |
| `github.com/shirou/gopsutil/v4` | (Optional) process name resolution |

### Frontend

| Package | Purpose |
|---|---|
| `svelte` | UI framework |
| `tailwindcss` | Utility CSS |
| `@tailwindcss/forms` | Form element resets |

## 10. Data Retention Policy

- `key_events`: default retention 30 days. Purge runs on app startup and daily via ticker.
- `daily_stats`: retained indefinitely (lightweight rows).
- User can override retention via Settings (future).
