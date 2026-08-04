package app

import (
	"context"
	"database/sql"
	"errors"
	"log"
	"os"
	"strings"

	"key-stats/internal/config"
	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/service"
	"key-stats/internal/stats"
	"key-stats/internal/win32"
	"key-stats/pkg/tray"

	"github.com/wailsapp/wails/v2/pkg/runtime"
	"golang.org/x/sys/windows/registry"
)

// App struct
type App struct {
	ctx           context.Context
	database      *db.DB
	keyboard      *service.KeyboardService
	trayMgr       *tray.Tray
	configManager *config.Manager
}

// NewApp creates a new App application struct
func NewApp(icon []byte) *App {
	return &App{
		trayMgr: tray.New(icon),
	}
}

// Startup is called when the app starts. Opens DB, starts logger, starts tray.
func (a *App) Startup(ctx context.Context) {
	a.ctx = ctx
	log.Println("App is starting up...")

	// 1. Resolve data directory (respects PORTABLE_MODE in .env)
	dataDir, err := config.DataDir()
	if err != nil {
		log.Printf("Failed to resolve data dir: %v", err)
		return
	}

	// 2. Initialize DB
	d, err := db.InitDB(dataDir)
	if err != nil {
		log.Printf("Failed to init DB: %v", err)
		return
	}
	a.database = d

	// 3. Start Keyboard Logger
	a.keyboard = service.NewKeyboardService(d)
	a.keyboard.Start()

	// 4. Load config once into memory (serializes reads/writes)
	a.configManager, _ = config.NewManager()

	// 5. Start system tray
	a.trayMgr.Run(ctx,
		func() { tray.ShowWindow(ctx) },
		func() { tray.QuitApp(ctx) },
	)
}

// Shutdown is called when the app is closing.
func (a *App) Shutdown(ctx context.Context) {
	log.Println("App is shutting down...")

	if a.trayMgr != nil {
		a.trayMgr.Quit()
	}
	if a.keyboard != nil {
		a.keyboard.Stop()
	}
	if a.database != nil {
		a.database.Close()
	}
}

// ApplyStartupOptions applies behavior flags from config (e.g. StartMinimized).
// Called once the window is ready so hiding works reliably.
func (a *App) ApplyStartupOptions() {
	m := a.ensureConfig()
	if m == nil {
		return
	}
	if m.Get().StartMinimized {
		runtime.WindowHide(a.ctx)
	}
}

// ensureConfig lazily initializes the in-memory config manager.
func (a *App) ensureConfig() *config.Manager {
	if a.configManager == nil {
		a.configManager, _ = config.NewManager()
	}
	return a.configManager
}

// -- Settings API --

// GetConfig returns the current application configuration as a flat map.
// The frontend can iterate this to render a dynamic settings UI.
func (a *App) GetConfig() (map[string]interface{}, error) {
	m := a.ensureConfig()
	if m == nil {
		return nil, errors.New("config unavailable")
	}
	cfg := m.Get()
	result := cfg.ToMap()
	if dir, err := config.DataDir(); err == nil {
		result["dataDir"] = dir
	}
	return result, nil
}

// SetConfig updates configuration fields from a flat map and persists to .env.
// Only keys present in the map are updated; others remain untouched.
// Returns the list of keys that were actually changed.
func (a *App) SetConfig(updates map[string]interface{}) ([]string, error) {
	m := a.ensureConfig()
	if m == nil {
		return nil, errors.New("config unavailable")
	}
	changed, err := m.Update(updates)
	if err != nil {
		return nil, err
	}
	for _, key := range changed {
		if key == "autoStart" {
			applyAutoStart(m.Get().AutoStart)
		}
	}
	return changed, nil
}

// SaveWindowSize persists the current window dimensions.
func (a *App) SaveWindowSize(width, height int) error {
	m := a.ensureConfig()
	if m == nil {
		return errors.New("config unavailable")
	}
	return m.SetWindowSize(width, height)
}

// SetPaused forwards the Live/Paused toggle to the keyboard recorder so that
// pausing actually stops recording, not just the frontend polling.
func (a *App) SetPaused(paused bool) {
	if a.keyboard != nil {
		a.keyboard.SetPaused(paused)
	}
}

// applyAutoStart registers or removes KeyStats in the Windows Run key.
func applyAutoStart(enabled bool) {
	const runKey = `Software\Microsoft\Windows\CurrentVersion\Run`
	k, err := registry.OpenKey(registry.CURRENT_USER, runKey, registry.QUERY_VALUE|registry.SET_VALUE)
	if err != nil {
		log.Printf("Failed to open Run key: %v", err)
		return
	}
	defer k.Close()

	if enabled {
		exe, err := os.Executable()
		if err != nil {
			log.Printf("Failed to resolve executable: %v", err)
			return
		}
		if err := k.SetStringValue("KeyStats", exe); err != nil {
			log.Printf("Failed to set autostart: %v", err)
		}
		return
	}
	if err := k.DeleteValue("KeyStats"); err != nil && err != registry.ErrNotExist {
		log.Printf("Failed to clear autostart: %v", err)
	}
}

// -- Stats API --

// conn returns the SQL connection, guarding against the not-yet-initialized case.
func (a *App) conn() (*sql.DB, error) {
	if a.database == nil {
		return nil, errors.New("database not initialized")
	}
	return a.database.GetConn(), nil
}

// GetTodayStats returns aggregate stats for the current day.
func (a *App) GetTodayStats() (models.TodaySummary, error) {
	conn, err := a.conn()
	if err != nil {
		return models.TodaySummary{}, err
	}
	return stats.GetDateRangeSummary(conn, 0, 0)
}

// GetDateRangeStats returns aggregate stats for a date range (inclusive).
// startDaysAgo is the older date, endDaysAgo is the newer date.
// Example: GetDateRangeStats(7, 0) returns stats for the last 7 days including today.
func (a *App) GetDateRangeStats(startDays, endDays int) (models.TodaySummary, error) {
	conn, err := a.conn()
	if err != nil {
		return models.TodaySummary{}, err
	}
	return stats.GetDateRangeSummary(conn, startDays, endDays)
}

// ResetStats clears all recorded keystroke statistics from the database.
func (a *App) ResetStats() error {
	if a.database == nil {
		return errors.New("database not initialized")
	}
	return a.database.Reset()
}

// -- Font API --

// GetSystemFonts returns a list of installed system font families.
// It reads from the Windows registry and falls back to common programming fonts.
func (a *App) GetSystemFonts() ([]string, error) {
	fonts := []string{
		"JetBrains Mono",
		"Fira Code",
		"Cascadia Code",
		"Cascadia Mono",
		"Source Code Pro",
		"Consolas",
		"Monaco",
		"Menlo",
		"SF Mono",
		"DejaVu Sans Mono",
		"Ubuntu Mono",
		"IBM Plex Mono",
		"Space Mono",
		"Inconsolata",
		"Courier New",
	}

	// Read Windows Fonts registry
	k, err := registry.OpenKey(registry.LOCAL_MACHINE,
		`SOFTWARE\Microsoft\Windows NT\CurrentVersion\Fonts`,
		registry.QUERY_VALUE)
	if err != nil {
		return fonts, nil // fallback to built-in list
	}
	defer k.Close()

	names, err := k.ReadValueNames(-1)
	if err != nil {
		return fonts, nil
	}

	seen := make(map[string]bool)
	for _, name := range names {
		// Registry values are like "JetBrains Mono Regular (TrueType)"
		// Extract font family name
		family := name
		if idx := strings.Index(name, " ("); idx > 0 {
			family = name[:idx]
		}
		if idx := strings.LastIndex(family, " Regular"); idx > 0 {
			family = family[:idx]
		}
		family = strings.TrimSpace(family)
		if family != "" && !seen[family] {
			seen[family] = true
			fonts = append(fonts, family)
		}
	}

	return fonts, nil
}

// SetWindowIcon loads the embedded icon and sets it on the Wails window.
func (a *App) SetWindowIcon() {
	hInstance, _, _ := win32.GetModuleHandleW.Call(0)
	if hInstance == 0 {
		return
	}

	const iconResID = 1
	hIcon, _, _ := win32.LoadImageW.Call(
		hInstance,
		uintptr(iconResID),
		uintptr(1), // IMAGE_ICON
		0, 0,
		uintptr(0x0040), // LR_DEFAULTSIZE
	)
	if hIcon == 0 {
		return
	}

	hwnd := win32.FindMainWindow()
	if hwnd == 0 {
		return
	}

	win32.SendMessageW.Call(hwnd, uintptr(0x0080), uintptr(1), hIcon) // WM_SETICON, ICON_BIG
	win32.SendMessageW.Call(hwnd, uintptr(0x0080), uintptr(0), hIcon) // WM_SETICON, ICON_SMALL
}
