# KeyStats — Code Map & Architecture Reference

## Part 1: System Architecture — Big Picture

```
                        WINDOWS KERNEL
                             │
                    WH_KEYBOARD_LL hook
                             │
              ┌──────────────┴──────────────┐
              │     hookProc (callback)      │  ← Windows callback thread
              │  internal/service/keyboard   │
              └──────────────┬──────────────┘
                             │
                    ┌────────┴────────┐
                    │                 │
              non-blocking        mutex.Lock()
              chan send           latestKeyName = name
                    │            latestKeyTs = ts
                    │            mutex.Unlock()
                    │                 │
              ┌─────┴─────┐    ┌─────┴──────────┐
              │  rtChan   │    │ GetLatestKey()  │ ← polled at 100ms
              │ (cap 256) │    │ returns (str,64) │
              └─────┬─────┘    └─────────────────┘
                    │
              rtEmitter()          [Frontend polls]
              drains chan              │
              (no emit used)     pollFlash() @100ms
                    │            GetLatestKeyPress()
                                 flashKey reactive var
                    │                 │
              ┌─────┴─────┐    ┌─────┴──────────┐
              │  eventChan │    │ KeyboardMap    │
              │ (cap 4096) │    │ $: flashKey    │
              └─────┬─────┘    │ → currentFlash  │
                    │          │ → key-flash CSS │
              batchWriter()    └────────────────┘
              @500ms or 256
                    │
              ┌─────┴─────┐
              │  SQLite    │
              │  data.db   │
              │  WAL mode  │
              └─────┬─────┘
                    │
              stats.go queries
              GetDateRangeSummary()
                    │
              ┌─────┴──────────┐
              │  [Frontend]    │
              │  poll @500ms   │
              │  fetchLiveStats│
              │  GetTodayStats │
              └───────┬────────┘
                      │
                statsData reactive
                      │
          ┌───────────┼───────────┐
          │           │           │
    totalKeys    topKeys[]   appBreakdown[]
      card        ranking       apps card
```

---

## Part 2: Go Backend — Package Tree & Function Map

```
main.go
 └── wails.Run()
      ├── config.Load()           → restore window size
      ├── OnStartup: App.Startup()
      └── Bind: [App struct]      → all exported methods → JS via Wails bridge

pkg/app/app.go  ─── App struct (12 exported methods)
│
├── [Lifecycle]
│   ├── Startup(ctx)              init DB → start keyboard → start tray
│   └── Shutdown(ctx)             stop tray → stop keyboard → close DB
│
├── [Config API]
│   ├── GetConfig()               config.Load() → ToMap() → map[string]any
│   ├── SetConfig(updates map)    config.Load() → UpdateFromMap() → config.Save()
│   └── SaveWindowSize(w, h)      config.Load() → save width/height → config.Save()
│
├── [Stats API]
│   ├── GetTodayStats()           stats.GetTodaySummary(db) → TodaySummary
│   ├── GetStats(daysAgo)         stats.GetStatsSummary(db, offset) → TodaySummary
│   ├── GetDateRangeStats(s, e)   stats.GetDateRangeSummary(db, start, end) → TodaySummary
│   └── ResetStats()              db.Reset() → DELETE FROM key_events
│
├── [Real-time API]
│   ├── GetLatestKeyPress()       keyboard.GetLatestKey() → {keyName, ts}
│   └── StartDrag()               drag_windows.go → SendMessage(WM_NCLBUTTONDOWN)
│
├── [Font API]
│   ├── GetSystemFonts()          registry read + fallback list → []string
│   └── SetWindowIcon()           Win32 LoadImage + SendMessage(WM_SETICON)
│
└── [Internal fields]
    ├── ctx       context.Context
    ├── database  *db.DB
    ├── keyboard  *service.KeyboardService
    └── trayMgr   *tray.Tray

internal/service/keyboard.go  ─── KeyboardService
│
├── [Win32 Hook Setup]
│   ├── NewKeyboardService(db)     constructor
│   ├── Start()                    goroutine: SetWindowsHookExW + message pump
│   └── Stop()                     UnhookWindowsHookEx + context cancel
│
├── [Real-time Pipeline]
│   ├── GetLatestKey()             mutex-guarded read of latestKeyName/ts
│   └── rtEmitter()                drains rtChan (no JS emit, avoids Svelte conflict)
│
├── [Global Callback]
│   └── hookProc(nCode, wParam, lParam)
│       ├── KBDLLHOOKSTRUCT        extract VkCode from lParam
│       ├── globalService.db.PushEvent(KeyEvent{...})
│       ├── getActiveWindowTitle() GetForegroundWindow + GetWindowTextW
│       ├── rtChan <- event        non-blocking send (drops if full)
│       └── latestKeyName/Ts       mutex-protected write
│
└── [Helper]
    └── getActiveWindowTitle()     Win32 GetForegroundWindow → GetWindowTextW

internal/db/sqlite.go  ─── DB
│
├── InitDB(dataDir)                sql.Open → PRAGMA WAL → CREATE TABLE → batchWriter()
├── PushEvent(e)                   non-blocking send to eventChan (cap 4096)
├── GetConn()                      returns *sql.DB for stats queries
├── Reset()                        DELETE FROM key_events
├── Close()                        cancel → drain channel → flush → close
│
├── [Internal Goroutine]
│   └── batchWriter()             ticker @500ms or batch≥256 → flush()
│       └── flush(batch)          BEGIN → PREPARE INSERT → stmt.Exec() → COMMIT
│
└── [Schema]
    CREATE TABLE key_events (
        id        INTEGER PRIMARY KEY AUTOINCREMENT,
        key_code  INTEGER NOT NULL,
        app_name  TEXT NOT NULL,
        timestamp INTEGER NOT NULL
    )
    INDEX idx_key_events_timestamp ON key_events(timestamp)
    INDEX idx_key_events_key_code ON key_events(key_code)

internal/stats/stats.go  ─── Query & Key Mapping
│
├── [Query Functions]
│   ├── GetTodaySummary(db)        → GetStatsSummary(db, 0)
│   ├── GetStatsSummary(db, days)  → GetDateRangeSummary(db, days, days)
│   └── GetDateRangeSummary(db, s, e)
│       ├── COUNT(*) total keys
│       ├── GROUP BY key_code → top 50 → aggregate by printable name → top 10
│       └── GROUP BY app_name → top 10 apps
│
└── [Key Code → Name Mapping]
    VKToName(vk int) string
    ├── 65-90   → A-Z
    ├── 48-57   → 0-9 (main row)
    ├── 96-105  → 0-9 (numpad → unified)
    ├── 112-135 → F1-F24
    ├── 160-165 → L/R Shift/Ctrl/Alt
    ├── Media / Browser / Navigation / Symbols
    └── 255,232,230 → Fn

internal/config/config.go  ─── .env-based Config
│
├── Load()        defaults() → parse .env lines → AppConfig{}
├── Save(cfg)     format .env with comments → WriteFile
├── DataDir()     portable: exe dir if .env there, else %APPDATA%/key-stats
├── defaults()    factory defaults (1280x800, dark, JetBrains Mono, etc.)
├── ToMap()       AppConfig → map[string]any (for JSON/API)
└── UpdateFromMap()  map → AppConfig fields, returns changed keys list

internal/models/models.go  ─── Data Structures
│
├── KeyEvent{ID, KeyCode, AppName, Timestamp}        ← raw event from hook
├── TodaySummary{TotalKeys, TopKeys[], AppBreakdown[]} ← API response
├── KeyCount{KeyCode, KeyName, Count}                 ← ranking item
└── AppCount{AppName, Count}                          ← app breakdown item

pkg/tray/tray_windows.go  ─── System Tray
│
└── getlantern/systray wrapper
    ├── Run(ctx, onShow, onQuit)
    └── Quit()
```

---

## Part 3: Frontend Component Tree

```
main.js (entry point)
 └── new App({ target: #app })
      │
      └── App.svelte ─── root component, owns all state
           │
           ├── [STATE]
           │   ├── statsData     ← {totalKeys, topKeys[], appBreakdown[]}
           │   ├── flashKey      ← {name, ts}  (from 100ms polling)
           │   ├── isLive        ← Live/Paused toggle
           │   ├── showMenu      ← dropdown/context menu visibility
           │   ├── modal*        ← modal state (show, title, message, mode, callbacks)
           │   ├── showSettingsPanel
           │   └── dateRange/startDays/endDays
           │
           ├── [LIFECYCLE — onMount]
           │   ├── GetConfig()              apply theme + font on load
           │   ├── fetchLiveStats()         initial data load
           │   ├── setInterval(fetch, 500)  stats polling (guarded by isLive)
           │   ├── setInterval(pollFlash, 100) flash polling
           │   ├── window resize listener   debounced SaveWindowSize()
           │   └── return cleanup           clearIntervals + removeListener
           │
           ├── [CHILD COMPONENTS]
           │   ├── KeyboardMap.svelte
           │   │   props: data={statsData.topKeys}  flashKey={flashKey}
           │   │
           │   ├── Modal.svelte
           │   │   props: show, title, message, mode, confirmText, cancelText
           │   │   events: on:confirm, on:cancel
           │   │
           │   └── SettingsPanel.svelte
           │       props: show
           │       (reads/writes config via GetConfig/SetConfig internally)
           │
           ├── [EVENTS]
           │   ├── on:click={handleMainClick}     close menus on outside click
           │   ├── on:keydown={handleMainKeydown}  Esc to close menu
           │   └── on:contextmenu|preventDefault  title bar context menu
           │
           └── [TITLE BAR]
               ├── Date range dropdown (Today / Yesterday / Last 7 / Last 30)
               ├── Live/Paused toggle button
               └── Menu button → dropdown menu
                    ├── Reset Records → Modal confirm
                    ├── Status → Modal info
                    ├── Settings → SettingsPanel
                    ├── Minimize to Tray → WindowHide()
                    └── Quit → Quit()

theme.js ─── Theme Manager (called from App.svelte onMount)
├── applyTheme(theme)        set data-theme attr + Wails native theme
├── setupAutoTheme(theme)    media query listener for auto mode
│   └── returns cleanup()
├── resolveTheme(theme)      auto → matchMedia, light/dark → direct
└── setNativeTheme(resolved) WindowSetLightTheme / WindowSetDarkTheme
```

---

## Part 4: Wails Bridge — JS ↔ Go Method Map

```
┌─────────────────────────────── JS Call Site ──────────────────────────────────────────────────────┐
│                                                                                                   │
│  window.go.app.App.GetConfig()          ──→  App.GetConfig()          → map[string]any            │
│  window.go.app.App.SetConfig(updates)   ──→  App.SetConfig(map)       → []string (changed keys)   │
│  window.go.app.App.SaveWindowSize(w,h)  ──→  App.SaveWindowSize(int,int) → error                  │
│                                                                                                   │
│  window.go.app.App.GetTodayStats()      ──→  App.GetTodayStats()      → models.TodaySummary       │
│  window.go.app.App.GetStats(days)       ──→  App.GetStats(int)        → models.TodaySummary       │
│  window.go.app.App.GetDateRangeStats(s,e)──→ App.GetDateRangeStats(int,int) → models.TodaySummary │
│  window.go.app.App.ResetStats()         ──→  App.ResetStats()         → error                     │
│                                                                                                   │
│  window.go.app.App.GetLatestKeyPress()  ──→  App.GetLatestKeyPress()  → {keyName, ts}             │
│  window.go.app.App.GetSystemFonts()     ──→  App.GetSystemFonts()     → []string                  │
│  window.go.app.App.SetWindowIcon()      ──→  App.SetWindowIcon()      → void (Win32 direct)       │
│  window.go.app.App.StartDrag()          ──→  App.StartDrag()          → void (Win32 direct)       │
│                                                                                                   │
└───────────────────────────────────────────────────────────────────────────────────────────────────┘

Auto-generated bindings (frontend/wailsjs/go/app/):
  App.js   — thin wrappers around window['go']['app']['App'][Method]()
  App.d.ts — TypeScript declarations for all exported methods

Runtime imports (from App.svelte):
  import { WindowHide, Quit } from '../wailsjs/runtime/runtime.js'
```

---

## Part 5: Data Flow — Detailed Sequence Diagrams

### 5.1 Keystroke Recording (Time-Critical Path)

```
USER PRESSES KEY

    ┌─ Windows Kernel ─┐
    │ WH_KEYBOARD_LL   │
    │ low-level hook    │
    └────────┬──────────┘
             │
    ┌────────▼──────────────────────────────────────────────────┐
    │ hookProc(nCode, wParam, lParam)    [callback thread]     │
    │                                                          │
    │ 1. KBDLLHOOKSTRUCT from lParam                           │
    │ 2. vk = int(kbd.VkCode)                                  │
    │                                                          │
    │ 3. globalService.db.PushEvent(KeyEvent{                  │
    │        KeyCode:   vk,                                    │
    │        AppName:   getActiveWindowTitle(),                │
    │        Timestamp: UnixNano() / 1ms                       │
    │    })                                                    │
    │    │                                                     │
    │    └──→ eventChan (cap 4096, non-blocking send)          │
    │                                                          │
    │ 4. name := stats.VKToName(vk)                            │
    │    rtChan <- {keyCode, keyName}   (non-blocking, 256)    │
    │    │                                                     │
    │    └──→ rtEmitter() drains (no emit, avoids conflicts)   │
    │                                                          │
    │ 5. mu.Lock()                                             │
    │    latestKeyName = name                                  │
    │    latestKeyTs = now_ms                                  │
    │    mu.Unlock()                                           │
    │                                                          │
    │ 6. CallNextHookEx → pass to next hook in chain           │
    └──────────────────────────────────────────────────────────┘

    ┌─ Batch Writer (goroutine) ─┐
    │ eventChan → batch buffer   │
    │ @500ms OR batch ≥ 256:     │
    │   BEGIN TRANSACTION        │
    │   INSERT INTO key_events   │
    │   COMMIT                   │
    └────────────────────────────┘
```

### 5.2 Stats Polling (Frontend Refresh @500ms)

```
App.svelte setInterval(500ms)

    isLive == true?
        │
    YES │
        ▼
    fetchLiveStats()
        │
        ├── startDays==0 && endDays==0?
        │       │
        │   YES ├──→ GetTodayStats()
        │   NO  └──→ GetDateRangeStats(start, end)
        │
        ▼
    [Wails Bridge] → Go → stats.GetTodaySummary(db)
        │
        ├── SELECT COUNT(*) WHERE date(timestamp/1000) = date('now')
        ├── SELECT key_code, COUNT(*) ... GROUP BY key_code ... LIMIT 50
        │   → aggregate by VKToName() → sort → top 10
        └── SELECT app_name, COUNT(*) ... GROUP BY app_name ... LIMIT 10
        │
        ▼
    TodaySummary{TotalKeys, TopKeys[], AppBreakdown[]}
        │
        ▼
    statsData = data   ← triggers Svelte reactivity
        │
        ├── totalKeys card updates
        ├── Top Keys ranking re-renders (progress bars animate)
        └── App Breakdown list re-renders
```

### 5.3 Key Flash Polling (Frontend @100ms)

```
App.svelte setInterval(100ms)

    pollFlash()
        │
        ├── GetLatestKeyPress()
        │       │
        │       ▼
        │   [Go] keyboard.GetLatestKey()
        │       → mutex.Lock → read latestKeyName/ts → mutex.Unlock
        │       → return {keyName, ts}
        │
        ├── data.ts !== lastPollTs ?    ← guard: only act on NEW keypress
        │       │
        │   YES │
        │       ▼
        │   lastPollTs = data.ts
        │   flashKey = { name: data.keyName, ts: data.ts }
        │       │
        │       ▼  (new object ref → Svelte reactivity)
        │   KeyboardMap.svelte  $: block
        │       │
        │       ├── flashKey.ts && flashKey.name && ts !== lastFlashTs ?
        │       │       │
        │       │   YES │
        │       │       ▼
        │       │   lastFlashTs = flashKey.ts
        │       │   currentFlash = normalizeKeyName(flashKey.name)
        │       │   flashRef.timer = setTimeout(180ms)
        │       │       │
        │       │       ▼
        │       │   CSS class "key-flash" added to matching key div
        │       │   (0.04s transition: scale 1.12, purple glow)
        │       │       │
        │       │       ▼  (after 180ms)
        │       │   currentFlash = ''   → key-flash class removed
        │       │
        │   NO  └── skip (same keypress, no update)
        │
        └── [Loop: repeats every 100ms]
```

### 5.4 Config Read/Write Flow

```
[READ] App loads
    │
    App.svelte onMount()
    └──→ GetConfig()
         └──→ config.Load()
              ├── resolve envPath()
              │   ├── .env next to exe? → portable mode
              │   └── else → %APPDATA%/key-stats/.env
              ├── parse key=value lines
              └── merge onto defaults()
              → AppConfig → ToMap() → map[string]any
         ← frontend applies theme + font

[WRITE] SettingsPanel change
    │
    SettingsPanel.svelte
    └──→ SetConfig({theme: "light"})
         └──→ config.Load() → UpdateFromMap() → config.Save()
              └── WriteFile(envPath(), formatted .env)
         ← returns ["theme"] (changed keys list)
```

---

## Part 6: Data Structures — Shape Reference

```
KeyEvent (Go: internal/models)          TodaySummary (Go → JSON → JS)
┌──────────────────────┐               ┌─────────────────────────────┐
│ ID        int64      │               │ totalKeys    number         │
│ KeyCode   int        │               │ topKeys      KeyCount[]     │
│ AppName   string     │               │ appBreakdown AppCount[]     │
│ Timestamp int64 (ms) │               └─────────────────────────────┘
└──────────────────────┘
                                               KeyCount
AppCount (part of TodaySummary)        ┌──────────────────────┐
┌──────────────────────┐               │ keyCode   number     │
│ appName   string     │               │ keyName   string     │
│ count     number     │               │ count     number     │
└──────────────────────┘               └──────────────────────┘

KeyboardMap.svelte props:
  data: KeyCount[]     ← statsData.topKeys
  flashKey: { name: string, ts: number }

AppConfig (Go: internal/config)
┌──────────────────────┐
│ windowWidth   int    │  → SaveWindowSize() persists
│ windowHeight  int    │  → SaveWindowSize() persists
│ theme         string │  → "dark" | "light" | "auto"
│ fontFamily    string │  → CSS --app-font variable
│ startMinimized bool  │
│ autoStart     bool   │
│ portableMode  bool   │  → detected from .env location
└──────────────────────┘
```

---

## Part 7: Svelte Reactive Dependency Graph

```
App.svelte  (reactive variables)
│
├── statsData ───────────────────────────┐
│   (updated by fetchLiveStats @500ms)   │
│                                        │
│   ┌────────────────────────────────────┘
│   ▼
│   Template binds:
│   ├── {statsData.totalKeys}              → total keys card
│   ├── {#each statsData.appBreakdown}     → app list
│   └── {#each statsData.topKeys}          → ranking list
│       └── props to KeyboardMap: data={statsData.topKeys}
│
├── flashKey ────────────────────────────┐
│   (updated by pollFlash @100ms,        │
│    only when data.ts changes)          │
│                                        │
│   ┌────────────────────────────────────┘
│   ▼
│   props to KeyboardMap: flashKey={flashKey}
│       │
│       ▼
│   KeyboardMap.svelte  $: block
│       $: if (flashKey.ts && flashKey.name && flashKey.ts !== lastFlashTs)
│           currentFlash = normalizeKeyName(...)
│           timeout → currentFlash = ''
│
├── isLive ──────────────────────────────┐
│   (toggled by Live/Paused button)      │
│                                        │
│   ├── CSS class on Live/Paused button  │
│   ├── Guard in fetch poll interval     │
│   └── Paused overlay (if applicable)   │
│
├── showMenu ────────────────────────────┐
│   (toggled by menu button/context)     │
│                                        │
│   └── {#if showMenu} renders dropdown  │
│
├── modalShow ───────────────────────────┐
│   (controlled by openModal/close)      │
│                                        │
│   └── props to Modal: bind:show        │
│
├── showSettingsPanel ──────────────────┐
│   (toggled by Settings menu item)      │
│                                        │
│   └── props to SettingsPanel: show     │
│
└── dateRange / startDays / endDays ─────┐
    (set by date dropdown selection)     │
                                         │
    └── dateFilter used in fetchLiveStats│
        → GetTodayStats vs GetDateRange  │
```

---

## Part 8: Critical Threading Model

```
┌─────────────────────────────────────────────────────────┐
│                    GOROUTINES                            │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  [Main Goroutine]                                       │
│    main.go → wails.Run()                                │
│    Blocks on WebView2 message loop                      │
│    All Wails-bound Go methods execute here              │
│    (GetTodayStats, GetLatestKeyPress, etc.)             │
│                                                         │
│  [Hook Message Pump Goroutine]                          │
│    keyboard.go → Start() goroutine                      │
│    SetWindowsHookExW → GetMessageW loop                 │
│    hookProc is CALLED BACK on Windows thread            │
│    (NOT a Go goroutine — syscall.NewCallback trampoline)│
│                                                         │
│  [Batch Writer Goroutine]                               │
│    db/sqlite.go → batchWriter()                         │
│    Ticker @500ms + batch≥256 flush                      │
│    Drains eventChan (cap 4096)                          │
│                                                         │
│  [rtEmitter Goroutine]                                  │
│    keyboard.go → rtEmitter()                            │
│    Drains rtChan to prevent backpressure                │
│    Does NOT call runtime.EventsEmit                     │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                    THREAD SAFETY                         │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  hookProc (Windows callback thread)                     │
│    ├── db.PushEvent()        non-blocking chan send     │
│    ├── rtChan send           non-blocking chan send     │
│    └── latestKeyName/Ts      sync.Mutex protected       │
│                                                         │
│  GetLatestKey() (main goroutine — Wails call)           │
│    └── sync.Mutex.Lock() to read latestKeyName/Ts       │
│                                                         │
│  config.Load/Save (main goroutine — Wails call)          │
│    └── File I/O — single-threaded access                │
│                                                         │
│  SQLite — modernc.org/sqlite (pure Go, no CGO)          │
│    └── WAL mode — concurrent reads safe                 │
│    └── Write via batch writer goroutine only            │
│                                                         │
└─────────────────────────────────────────────────────────┘
```
