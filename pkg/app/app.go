package app

import (
	"context"
	"fmt"
	"log"
	"strings"
	"syscall"
	"unsafe"

	"key-stats/internal/config"
	"key-stats/internal/db"
	"key-stats/internal/models"
	"key-stats/internal/service"
	"key-stats/internal/stats"
	"key-stats/pkg/tray"

	"golang.org/x/sys/windows/registry"
)

// App struct
type App struct {
	ctx      context.Context
	database *db.DB
	keyboard *service.KeyboardService
	trayMgr  *tray.Tray
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

	// 4. Start system tray
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

// -- Settings API (extensible) --

// GetConfig returns the current application configuration as a flat map.
// The frontend can iterate this to render a dynamic settings UI.
func (a *App) GetConfig() (map[string]interface{}, error) {
	cfg, err := config.Load()
	if err != nil {
		return nil, err
	}
	return cfg.ToMap(), nil
}

// SetConfig updates configuration fields from a flat map and persists to .env.
// Only keys present in the map are updated; others remain untouched.
// Returns the list of keys that were actually changed.
func (a *App) SetConfig(updates map[string]interface{}) ([]string, error) {
	cfg, err := config.Load()
	if err != nil {
		return nil, err
	}
	changed := cfg.UpdateFromMap(updates)
	if len(changed) > 0 {
		if err := config.Save(cfg); err != nil {
			return nil, err
		}
	}
	return changed, nil
}

// SaveWindowSize persists the current window dimensions.
func (a *App) SaveWindowSize(width, height int) error {
	cfg, err := config.Load()
	if err != nil {
		return err
	}
	cfg.WindowWidth = width
	cfg.WindowHeight = height
	return config.Save(cfg)
}

// -- Stats API --

// GetTodayStats returns aggregate stats for the current day.
func (a *App) GetTodayStats() (models.TodaySummary, error) {
	if a.database == nil {
		return models.TodaySummary{}, fmt.Errorf("database not initialized")
	}
	return stats.GetTodaySummary(a.database.GetConn())
}

// GetStats returns aggregate stats for a given day offset (0=today, 1=yesterday, etc.).
func (a *App) GetStats(daysAgo int) (models.TodaySummary, error) {
	if a.database == nil {
		return models.TodaySummary{}, fmt.Errorf("database not initialized")
	}
	return stats.GetStatsSummary(a.database.GetConn(), daysAgo)
}

// GetDateRangeStats returns aggregate stats for a date range (inclusive).
// startDaysAgo is the older date, endDaysAgo is the newer date.
// Example: GetDateRangeStats(7, 0) returns stats for the last 7 days including today.
func (a *App) GetDateRangeStats(startDays, endDays int) (models.TodaySummary, error) {
	if a.database == nil {
		return models.TodaySummary{}, fmt.Errorf("database not initialized")
	}
	return stats.GetDateRangeSummary(a.database.GetConn(), startDays, endDays)
}

// ResetStats clears all recorded keystroke statistics from the database.
func (a *App) ResetStats() error {
	if a.database == nil {
		return fmt.Errorf("database not initialized")
	}
	return a.database.Reset()
}

// GetLatestKeyPress returns the most recent key press (name + timestamp) for
// real-time flash feedback on the keyboard heatmap. Returns zero values if no
// key has been pressed yet.
func (a *App) GetLatestKeyPress() map[string]interface{} {
	if a.keyboard == nil {
		return map[string]interface{}{"keyName": "", "ts": int64(0)}
	}
	name, ts := a.keyboard.GetLatestKey()
	return map[string]interface{}{"keyName": name, "ts": ts}
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
	user32 := syscall.NewLazyDLL("user32.dll")
	kernel32 := syscall.NewLazyDLL("kernel32.dll")
	procLoadImageW := user32.NewProc("LoadImageW")
	procFindWindowW := user32.NewProc("FindWindowW")
	procSendMessageW := user32.NewProc("SendMessageW")
	procGetModuleHandleW := kernel32.NewProc("GetModuleHandleW")

	hInstance, _, _ := procGetModuleHandleW.Call(0)
	if hInstance == 0 {
		return
	}

	const iconResID = 1
	hIcon, _, _ := procLoadImageW.Call(
		hInstance,
		uintptr(iconResID),
		uintptr(1), // IMAGE_ICON
		0, 0,
		uintptr(0x0040), // LR_DEFAULTSIZE
	)
	if hIcon == 0 {
		return
	}

	className, _ := syscall.UTF16PtrFromString("wailsWindow")
	hwnd, _, _ := procFindWindowW.Call(
		uintptr(unsafe.Pointer(className)),
		0,
	)
	if hwnd == 0 {
		return
	}

	procSendMessageW.Call(hwnd, uintptr(0x0080), uintptr(1), hIcon) // WM_SETICON, ICON_BIG
	procSendMessageW.Call(hwnd, uintptr(0x0080), uintptr(0), hIcon) // WM_SETICON, ICON_SMALL
}


